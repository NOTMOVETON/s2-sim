#pragma once

/**
 * @file actor_behavior.hpp
 * IActorBehavior — базовый интерфейс поведения актора.
 *
 * Актор (дверь, конвейер, лифт, пешеход) имеет один behavior,
 * который определяет его логику и визуальное состояние.
 *
 * Behavior императивно управляет геометрией и коллизией актора:
 *  - update() принимает изменяемую ссылку на Actor
 *  - behavior напрямую двигает world_pose, collision, visual
 *  - публикует ActorStateChanged через EventBus при смене состояния FSM
 *
 * WorldContext — контекст ядра, передаваемый в behavior.update().
 * Аналогичен PluginContext для плагинов агентов.
 *
 * SignalEvent — событие сигнала для on_signal().
 * Используется wire-контроллерами и другими источниками сигналов.
 */

#include <s2/event_bus.hpp>
#include <s2/kernel_command.hpp>
#include <s2/types.hpp>
#include <s2/world_query.hpp>

#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>

#include <memory>
#include <string>

namespace s2
{

// Forward-declare Actor — избегает circular include с actor.hpp
struct Actor;

// ============================================================================
// WorldContext — контекст ядра для behavior.update()
// ============================================================================

/**
 * @brief Контекст, передаваемый в IActorBehavior::update().
 *
 * Аналог PluginContext, но для акторов.
 * Предоставляет read-only доступ к миру, шину событий и очередь команд.
 *
 * sim_time — текущее симуляционное время (секунды от начала симуляции).
 */
struct WorldContext
{
  const WorldQuery&   world;     ///< Read-only доступ к миру
  EventBus&           bus;       ///< Публикация событий (ActorStateChanged и т.п.)
  KernelCommandQueue& commands;  ///< Очередь команд ядра
  double              sim_time;  ///< Текущее симуляционное время (секунды)
};

// ============================================================================
// SignalEvent — событие сигнала
// ============================================================================

/**
 * @brief Событие сигнала для IActorBehavior::on_signal().
 *
 * Доставляется behavior'у при активации/деактивации wire-сигнала
 * или другого сигнала, адресованного данному актору.
 *
 * active = true  — сигнал активирован
 * active = false — сигнал деактивирован
 */
struct SignalEvent
{
  std::string signal_id;       ///< Идентификатор сигнала
  EntityId    source_entity{0}; ///< Entity-источник сигнала
  bool        active{true};     ///< true = активирован, false = деактивирован
};

// ============================================================================
// IActorBehavior — базовый интерфейс поведения актора
// ============================================================================

/**
 * @brief Базовый интерфейс поведения актора.
 *
 * Определяет полный lifecycle актора:
 *  - on_init()    — загрузка конфигурации из YAML при загрузке сцены
 *  - on_spawn()   — вызывается после добавления актора в мир (можно сохранить id)
 *  - on_reset()   — сброс к начальному состоянию при ResetSim
 *  - update()     — основной тиковый метод (двигает геометрию, обновляет FSM)
 *  - on_signal()  — реакция на wire-сигнал или другой сигнал
 *  - on_interact()— реакция на Interact KernelCommand (открыть дверь, вызвать лифт)
 *
 * Behavior публикует ActorStateChanged в EventBus самостоятельно
 * при смене состояния FSM (ядро не отслеживает current_state() автоматически).
 *
 * Стабы Phase 7 (MaterialTransfer / DeformEntity) возвращают false —
 * переопределяются конкретными behavior'ами при необходимости.
 */
class IActorBehavior
{
public:
    virtual ~IActorBehavior() = default;

    // ─── Идентификация ───────────────────────────────────────────────────────

    /**
     * @brief Тип поведения: "door", "conveyor", "elevator", "pedestrian".
     * Используется для фабрики и отладки.
     */
    virtual std::string type() const = 0;

    // ─── Lifecycle ───────────────────────────────────────────────────────────

    /**
     * @brief Инициализация из YAML-конфигурации при загрузке сцены.
     * Вызывается один раз при загрузке. Не вызывается при reset.
     * @param config YAML-нода с параметрами поведения
     */
    virtual void on_init(const YAML::Node& config) = 0;

    /**
     * @brief Вызывается после добавления актора в мир.
     * Behavior может сохранить actor_id для последующего использования.
     * По умолчанию — no-op.
     * @param actor_id Идентификатор актора в мире
     */
    virtual void on_spawn(ActorId actor_id) { (void)actor_id; }

    /**
     * @brief Сброс к начальному состоянию при ResetSim.
     * По умолчанию — no-op.
     */
    virtual void on_reset() {}

    // ─── Основной тик ────────────────────────────────────────────────────────

    /**
     * @brief Основной тиковый метод поведения.
     *
     * Behavior императивно управляет актором:
     *  - Двигает actor.world_pose
     *  - Обновляет actor.collision_enabled
     *  - Публикует события через ctx.bus
     *  - Добавляет команды в ctx.commands
     *
     * @param dt    Шаг тика (секунды)
     * @param actor Изменяемая ссылка на актора (behavior двигает геометрию)
     * @param ctx   Контекст ядра: world, bus, commands, sim_time
     */
    virtual void update(double dt, Actor& actor, const WorldContext& ctx) = 0;

    // ─── Сигналы и взаимодействия ────────────────────────────────────────────

    /**
     * @brief Реакция на wire-сигнал или другой сигнал.
     * Вызывается controller-плагином (DoorWireController) или ядром.
     * По умолчанию — no-op.
     * @param event Событие сигнала (signal_id, source_entity, active)
     */
    virtual void on_signal(const SignalEvent& event) { (void)event; }

    /**
     * @brief Реакция на Interact KernelCommand.
     *
     * Ядро маршрутизирует cmd::Interact к behavior целевого актора.
     * Примеры действий: "open", "close", "call", "push".
     *
     * По умолчанию — no-op.
     *
     * @param source Entity-источник взаимодействия (агент, актор)
     * @param action Имя действия (строка из whitelist behavior)
     * @param params Произвольные параметры действия (JSON)
     */
    virtual void on_interact(EntityId source, const std::string& action,
                             const nlohmann::json& params)
    {
        (void)source;
        (void)action;
        (void)params;
    }

    // ─── Состояние и сериализация ────────────────────────────────────────────

    /**
     * @brief Текущее состояние FSM как строка.
     * Используется для snapshot'ов и отладки.
     * Примеры: "closed", "opening", "open", "closing".
     */
    virtual std::string current_state() const = 0;

    /**
     * @brief Сериализация состояния behavior в JSON.
     * Используется для снапшотов визуализатора и отладки.
     */
    virtual std::string to_json() const = 0;

    // ─── Стабы Phase 7 (MaterialTransfer / DeformEntity) ─────────────────────

    /**
     * @brief Может ли актор отдать материал (Phase 7: DirtPile, Container).
     * По умолчанию — false.
     */
    virtual bool can_release_material() const { return false; }

    /**
     * @brief Может ли актор принять материал (Phase 7: Container, Truck).
     * По умолчанию — false.
     */
    virtual bool can_accept_material() const { return false; }

    /**
     * @brief Является ли актор деформируемым (Phase 7: DirtPile).
     * По умолчанию — false.
     */
    virtual bool is_deformable() const { return false; }
};

}  // namespace s2
