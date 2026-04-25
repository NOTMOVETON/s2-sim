#pragma once
/**
 * @file kernel_command.hpp
 * KernelCommand — единственный способ изменить мир из плагинов и REST API.
 *
 * Все изменения мира идут через KernelCommand. Никаких прямых вызовов
 * world.entities.push_back() или agent.plugins.push_back() из внешнего кода.
 *
 * Команды применяются SimEngine в Phase 0 каждого тика атомарно.
 * Плагины складывают команды в PluginContext::commands (однотиковый буфер).
 * REST API / VizServer складывают команды в SimEngine::command_queue_ (mutex-защита).
 *
 * Для добавления новой команды:
 *  1. Определить struct в namespace s2::cmd
 *  2. Добавить тип в using KernelCommand = std::variant<...>
 *  3. Добавить обработчик в SimEngine::phase0_kernel_commands()
 */

#include <s2/types.hpp>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace s2
{

// ============================================================================
// Команды ядра
// ============================================================================

namespace cmd
{

// ─── Entity lifecycle ─────────────────────────────────────────────────────────

/**
 * @brief Создать новую Entity.
 * @param entity_type Тип: "agent", "actor", "prop"
 * @param config_yaml YAML-строка с конфигурацией Entity (как в сцене)
 */
struct SpawnEntity
{
  std::string entity_type;  ///< "agent", "actor", "prop"
  std::string config_yaml;  ///< YAML конфиг Entity (имя, pose, плагины и т.п.)
};

/**
 * @brief Удалить Entity из симуляции.
 * Вызывает on_despawn() для всех плагинов перед удалением.
 */
struct DespawnEntity
{
  EntityId id;
};

/**
 * @brief Установить позу Entity.
 */
struct SetPose
{
  EntityId id;
  Pose3D   pose;
};

/**
 * @brief Включить или выключить Entity.
 * Выключенная Entity не участвует в зонах, коллизиях и обновлении.
 */
struct SetEnabled
{
  EntityId id;
  bool     enabled{true};
};

// ─── Plugin lifecycle (hot reload) ────────────────────────────────────────────

/**
 * @brief Добавить плагин к агенту.
 * Вызывает on_spawn(agent) у добавленного плагина.
 * Валидация: если у агента уже есть ACTUATION плагин и новый тоже ACTUATION — ошибка.
 */
struct AddPlugin
{
  EntityId    entity_id;
  std::string plugin_type;  ///< Тип плагина: "diff_drive", "lidar", "battery" и т.п.
  YAML::Node  config;       ///< Конфигурация (как в YAML сцены)
};

/**
 * @brief Удалить плагин у агента.
 * Вызывает on_despawn(agent) у удаляемого плагина.
 */
struct RemovePlugin
{
  EntityId    entity_id;
  std::string plugin_type;
};

/**
 * @brief Переконфигурировать плагин у агента без перезапуска.
 * Вызывает from_config(new_config) + initialize(agent).
 */
struct ConfigPlugin
{
  EntityId    entity_id;
  std::string plugin_type;
  YAML::Node  new_config;
};

// ─── Zones ────────────────────────────────────────────────────────────────────

/**
 * @brief Создать новую зону.
 * @param attached_to Если задан — зона следует за этой Entity
 */
struct SpawnZone
{
  ZoneShape                shape;
  std::vector<std::string> effects;         ///< Список типов эффектов: ["ice", "fog"]
  std::optional<EntityId>  attached_to;     ///< Привязать к Entity (движется вместе)
  std::string              id_hint;         ///< Желаемый ZoneId (пустой = auto-generated)
  bool                     visible{true};
  std::string              color{"#FFFFFF"};
  double                   opacity{0.3};
  std::string              label;
};

/**
 * @brief Удалить зону.
 */
struct DespawnZone
{
  ZoneId id;
};

/**
 * @brief Включить или выключить зону.
 * Выключение: все Entity внутри получают ZoneExited событие.
 * Включение: все Entity внутри получают ZoneEntered заново.
 */
struct ToggleZone
{
  ZoneId id;
  bool   enabled{true};
};

// ─── Interactions ─────────────────────────────────────────────────────────────

/**
 * @brief Единая точка взаимодействия Entity с Entity.
 *
 * Ядро валидирует:
 *  - source_id и target_id существуют
 *  - дистанция допустима (если max_distance > 0)
 *  - capabilities target подходят
 *  - action валиден для target.behavior
 * Затем вызывает target.behavior.on_interact(source, action, params).
 */
struct Interact
{
  EntityId       source_id;
  EntityId       target_id;
  std::string    action;              ///< Имя действия: "open", "grab", "call", "push"
  nlohmann::json params;              ///< Произвольные параметры действия
  double         max_distance{0.0};  ///< 0 = без ограничения дистанции
};

/**
 * @brief Привязать один объект к другому (parent-child иерархия).
 * После привязки child следует за parent (обновляется в Phase 6 тика).
 */
struct AttachObject
{
  EntityId    parent_id;
  std::string link;        ///< Имя link-а родителя ("gripper", "base_link")
  EntityId    child_id;
  Pose3D      local_pose;  ///< Смещение child относительно parent link
};

/**
 * @brief Отсоединить объект от родителя.
 * @param drop_pose Если задан — установить эту позу после отсоединения
 */
struct DetachObject
{
  EntityId              child_id;
  std::optional<Pose3D> drop_pose;
};

// ─── Scenes ───────────────────────────────────────────────────────────────────

/** @brief Загрузить сцену из файла. Полный перезапуск. */
struct LoadScene { std::string name; };

/** @brief Сохранить текущую сцену в файл. */
struct SaveScene { std::string name; };

/** @brief Создать пустую новую сцену. */
struct NewScene {};

}  // namespace cmd

// ============================================================================
// KernelCommand — единый тип для всех команд
// ============================================================================

/**
 * @brief Единый тип KernelCommand.
 *
 * std::variant перечисляет все возможные команды.
 * SimEngine::phase0_kernel_commands() использует std::visit для обработки.
 *
 * Пример создания команды:
 * @code
 * ctx.commands.push_back(cmd::SetPose{.id = agent.id, .pose = new_pose});
 * ctx.commands.push_back(cmd::Interact{.source_id = 1, .target_id = 2, .action = "open"});
 * @endcode
 */
using KernelCommand = std::variant<
  cmd::SpawnEntity,
  cmd::DespawnEntity,
  cmd::SetPose,
  cmd::SetEnabled,
  cmd::AddPlugin,
  cmd::RemovePlugin,
  cmd::ConfigPlugin,
  cmd::SpawnZone,
  cmd::DespawnZone,
  cmd::ToggleZone,
  cmd::Interact,
  cmd::AttachObject,
  cmd::DetachObject,
  cmd::LoadScene,
  cmd::SaveScene,
  cmd::NewScene
>;

/// Очередь команд ядра — однотиковый локальный буфер плагина или тиковый буфер SimEngine.
using KernelCommandQueue = std::vector<KernelCommand>;

}  // namespace s2
