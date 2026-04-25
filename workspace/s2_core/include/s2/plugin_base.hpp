#pragma once

/**
 * @file plugin_base.hpp
 * Базовый интерфейс для плагинов агента.
 *
 * Живёт в s2_core, потому что Agent хранит unique_ptr<IAgentPlugin>
 * и SimEngine вызывает plugin->update(dt, agent, ctx).
 * Реестр фабрик — в s2_plugins.
 */

#include <s2/event_bus.hpp>
#include <s2/transport_adapter.hpp>
#include <s2/world_query.hpp>
#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace s2
{

// Forward-declare
struct Agent;
class CollisionSystem;
class RaycastEngine;
class SimWorld;

/**
 * @brief Команда ядра симуляции.
 *
 * Placeholder-определение для Plan 02. Полный вариант с std::variant<...>
 * будет добавлен в kernel_command.hpp (Plan 04).
 *
 * Позволяет PlaginContext::commands передавать очередь команд без
 * полного определения всех типов команд.
 */
struct KernelCommand
{
  // TODO(Plan04): заменить на std::variant<cmd::SpawnEntity, cmd::DespawnEntity, ...>
  // Пока — пустой тип для compileability PluginContext.
};

/// Очередь команд ядра — однотиковый локальный буфер плагина.
/// Плагин добавляет команды, SimEngine применяет их в Phase 0 следующего тика.
using KernelCommandQueue = std::vector<KernelCommand>;

/**
 * @brief Роль плагина агента в системе.
 *
 * Определяет к каким ресурсам плагин имеет право доступа:
 *  ACTUATION   — читает SharedState, пишет agent.velocity (движение тела)
 *  SENSOR      — читает WorldQuery, публикует данные на транспорт
 *  INTERACTION — читает WorldQuery, публикует KernelCommand и EventBus
 *  RESOURCE    — пишет в SharedState (battery, payload)
 *  UTILITY     — вспомогательные действия (joints, trajectory recorder)
 *
 * Ограничение: у агента максимум один ACTUATION-плагин (валидируется при загрузке).
 */
enum class PluginRole
{
  ACTUATION,    ///< Пишет в agent.velocity (DiffDrive, GravityPlugin)
  SENSOR,       ///< Читает WorldQuery, публикует данные (Lidar, GNSS, IMU)
  INTERACTION,  ///< Выдаёт KernelCommand / EventBus (Grabber, DoorOpener)
  RESOURCE,     ///< Пишет в SharedState (Battery, Payload)
  UTILITY       ///< Вспомогательные (JointVel, TrajectoryRecorder, Color)
};

/**
 * @brief Контекст плагина — доступ к инфраструктуре ядра.
 *
 * Передаётся в update() каждого плагина каждый тик.
 * Позволяет плагинам взаимодействовать с ядром без прямой зависимости.
 *
 * world    — read-only доступ к миру (поиск Entity, сигналы, raycast, зоны)
 * bus      — публикация событий (только publish, не subscribe)
 * commands — очередь команд ядра (SpawnEntity, Interact, AttachObject и т.п.)
 *
 * KernelCommandQueue — однотиковый локальный буфер. Плагин добавляет команды,
 * SimEngine применяет их в Phase 0 следующего тика.
 */
struct PluginContext
{
  const WorldQuery&   world;     ///< Read-only доступ к миру
  EventBus&           bus;       ///< Шина событий (publish)
  KernelCommandQueue& commands;  ///< Очередь команд ядра
};

namespace plugins
{

/**
 * @brief Базовый интерфейс плагина агента.
 *
 * Каждый плагин имеет:
 *  - Основные методы: type(), update(), from_config(), to_json()
 *  - Опциональные методы входа: has_inputs(), inputs_schema(), handle_input()
 *  - Опциональные методы транспорта: command_topics(), service_names(),
 *    handle_service(), poll_events()
 *
 * Методы входа позволяют плагину принимать команды из внешних источников
 * (VizUI, ROS2, MQTT и т.д.) через единый интерфейс handle_input(json).
 */
class IAgentPlugin
{
public:
    virtual ~IAgentPlugin() = default;

    // ─── Имя экземпляра плагина ────────────────────────────────────────────
    //
    // Если один агент имеет два плагина одного типа (например, два GNSS),
    // имя используется как суффикс топика: gnss "" → /gnss/fix,
    // gnss "left" → /gnss/left/fix.

    const std::string& sensor_name() const { return sensor_name_; }
    void set_sensor_name(const std::string& name) { sensor_name_ = name; }

    // ─── Частота публикации ────────────────────────────────────────────────

    /**
     * @brief Желаемая частота публикации данных этого плагина (Гц).
     * Возвращает base_rate_hz_ если он задан (переопределён из YAML),
     * иначе возвращает default_publish_rate_hz() плагина.
     * 0 — публиковать при каждом вызове on_post_tick (определяется transport_rate).
     */
    virtual double publish_rate_hz() const
    {
        return base_rate_hz_ > 0.0 ? base_rate_hz_ : default_publish_rate_hz();
    }

    /**
     * @brief Частота публикации по умолчанию (Гц), задаётся конкретным плагином.
     * Переопределяется в подклассах. Например, GnssPlugin возвращает 10.0.
     */
    virtual double default_publish_rate_hz() const { return 0.0; }

    /**
     * @brief Установить частоту публикации из YAML (побеждает default_publish_rate_hz).
     * Вызывается SceneLoader'ом при наличии поля publish_rate_hz: в конфиге плагина.
     */
    void set_base_rate(double hz) { base_rate_hz_ = hz; }

    // ─── Выходной топик ───────────────────────────────────────────────────

    /**
     * @brief Получить переопределённое имя выходного топика.
     * Если пустой — транспортный адаптер использует конвенцию именования.
     */
    const std::string& output_topic() const { return output_topic_; }

    /**
     * @brief Установить имя выходного топика из YAML.
     * Вызывается SceneLoader'ом при наличии поля topic: в конфиге плагина.
     */
    void set_output_topic(const std::string& t) { output_topic_ = t; }

    // ─── Основные методы ───

    virtual std::string type() const = 0;

    /**
     * @brief Роль плагина в системе.
     * Используется для валидации (ACTUATION = max 1 на агента) и маршрутизации.
     */
    virtual PluginRole role() const = 0;

    /**
     * @brief Список capabilities, которые плагин автоматически добавляет Entity.
     * Используется WorldQuery::EntityFilter::required_capabilities.
     * По умолчанию — пустой список.
     */
    virtual std::vector<std::string> provided_capabilities() const { return {}; }

    /**
     * @brief Инициализировать плагин после загрузки агента.
     * Вызывается из SimTransportBridge::init() после регистрации всех плагинов.
     * Позволяет плагину запомнить начальное состояние агента (например, цвет).
     */
    virtual void initialize(Agent& agent) { (void)agent; }

    // ─── Lifecycle-методы плагина ──────────────────────────────────────────────

    /**
     * @brief Вызывается при добавлении плагина к Entity (spawn или AddPlugin команда).
     * Используется для инициализации, которая требует доступа к Entity-контексту.
     * По умолчанию — no-op.
     */
    virtual void on_spawn(Agent& agent) { (void)agent; }

    /**
     * @brief Вызывается при удалении плагина (DespawnEntity или RemovePlugin команда).
     * Используется для освобождения ресурсов, публикации финальных событий.
     * По умолчанию — no-op.
     */
    virtual void on_despawn(Agent& agent) { (void)agent; }

    /**
     * @brief Вызывается при загрузке новой сцены (LoadScene или NewScene команда).
     * Позволяет плагину прочитать начальное состояние мира.
     * По умолчанию — no-op.
     */
    virtual void on_scene_load(const SimWorld& world) { (void)world; }

    /**
     * @brief Вызывается при сбросе симуляции (ResetSim команда).
     * Плагин должен сбросить своё внутреннее состояние к начальному.
     * По умолчанию — no-op.
     */
    virtual void on_reset(Agent& agent) { (void)agent; }

    /**
     * @brief Ранняя фаза тика — до resolve() contributions.
     *
     * Вызывается SimEngine в фазе 3a (Resource modules), до zone_effects и до resolve().
     * Используется плагинами, которым нужно публиковать contributions в SharedState
     * так, чтобы они были учтены resolve() в этом же тике.
     *
     * Типичное применение: BatteryPlugin разряжает батарею и публикует
     * add_scale() / add_lock() чтобы DiffDrive прочитал ограничения через effective().
     *
     * По умолчанию — no-op. Плагины, которым не нужна ранняя фаза, не переопределяют.
     */
    virtual void pre_resolve(double /*dt*/, Agent& /*agent*/) {}

    /**
     * @brief Передать ссылку на CollisionSystem перед вызовом update().
     * Вызывается SimEngine в фазе 3e для каждого плагина каждый тик.
     * Плагины, которым нужна CollisionSystem (например, GravityPlugin),
     * переопределяют этот метод. Остальные игнорируют вызов.
     */
    virtual void set_collision_system(const CollisionSystem*) {}

    /**
     * @brief Передать ссылку на RaycastEngine перед вызовом update().
     * Вызывается SimEngine в фазе 3e для каждого плагина каждый тик.
     * Плагины, которым нужен RaycastEngine (например, LidarPlugin),
     * переопределяют этот метод. Остальные игнорируют вызов.
     */
    virtual void set_raycast_engine(const RaycastEngine*) {}

    /**
     * @brief TF-фрейм, в котором установлен сенсор (для заголовка ROS2-сообщений).
     * Пустая строка означает, что плагин не задаёт фрейм явно.
     * Переопределяется плагинами с определённым монтажным фреймом (например, LidarPlugin).
     */
    virtual std::string sensor_frame_id() const { return ""; }

    /**
     * @brief Основной тиковый метод плагина.
     * @param dt   Длительность тика (секунды)
     * @param agent Агент, которому принадлежит плагин
     * @param ctx  Контекст ядра: WorldQuery, EventBus, KernelCommandQueue
     */
    virtual void update(double dt, Agent& agent, const PluginContext& ctx) = 0;
    virtual void from_config(const YAML::Node& node) = 0;
    virtual std::string to_json() const = 0;

    /**
     * @brief Добавить данные плагина в снапшот агента.
     *
     * Вызывается SimEngine::build_snapshot() для каждого плагина каждого агента.
     * Плагины записывают свои поля прямо в out (JSON-объект).
     * Ключи из разных плагинов объединяются.
     *
     * Пример реализации в BatteryPlugin:
     *   out["battery_level"]    = bat->level;
     *   out["battery_charging"] = bat->charging;
     *
     * По умолчанию ничего не добавляет.
     */
    virtual void contribute_snapshot(nlohmann::json& /*out*/, const Agent& /*agent*/) const {}

    // ─── Схема конфигурации для UI-редактора сцены ───

    /**
     * @brief Человекочитаемое название плагина для UI-редактора.
     * Используется в списке доступных плагинов при добавлении агента.
     * По умолчанию возвращает type().
     */
    virtual std::string display_label() const { return type(); }

    /**
     * @brief Схема параметров конфигурации плагина для UI-редактора.
     *
     * Возвращает JSON Schema (массив параметров конфигурации):
     * [{"key":"...", "label":"...", "type":"number|text|color", "default":...}, ...]
     *
     * По умолчанию возвращает пустой массив (плагин без параметров).
     * Переопределяется плагинами с конфигурируемыми параметрами.
     */
    virtual nlohmann::json config_schema() const { return nlohmann::json::array(); }

    // ─── Методы входа (input handling) ───

    /**
     * @brief Принимает ли плагин внешние команды?
     * По умолчанию false — плагин только отдаёт данные (сенсоры).
     */
    virtual bool has_inputs() const { return false; }

    /**
     * @brief JSON Schema входных данных плагина.
     * Используется для генерации UI-форм и валидации на сервере.
     * Пример: {"linear_velocity": {"type": "number", "default": 0, "min": -2, "max": 2}}
     */
    virtual std::string inputs_schema() const { return ""; }

    /**
     * @brief Обработать внешнюю команду.
     * @param json_input JSON-строка с входными данными (валидируется по inputs_schema)
     * Вызывается транспортом (VizUI, ROS2, MQTT) через SimEngine::handle_plugin_input().
     */
    virtual void handle_input(const std::string& json_input) { (void)json_input; }

    // ─── Методы транспортного взаимодействия ───

    /**
     * @brief Топики команд, которые хочет получать плагин.
     * Транспортный адаптер подпишется на эти топики и будет вызывать handle_input().
     * Пример: DiffDrivePlugin возвращает {"/cmd_vel"}, JointVelPlugin — {"/cmd_vel_mount"}.
     */
    virtual std::vector<std::string> command_topics() const { return {}; }

    /**
     * @brief Имена сервисов, которые предоставляет плагин.
     * Транспортный адаптер зарегистрирует эти сервисы и будет вызывать handle_service().
     * Пример: GrabberPlugin возвращает {"/grab"}.
     */
    virtual std::vector<std::string> service_names() const { return {}; }

    /**
     * @brief Обработать вызов сервиса.
     * @param service_name Имя сервиса из service_names()
     * @param request_json JSON-строка с параметрами запроса
     * @return JSON-строка с ответом (минимум: {"success": true/false})
     */
    virtual std::string handle_service(const std::string& service_name,
                                       const std::string& request_json)
    {
        (void)service_name;
        (void)request_json;
        return "{\"success\":false,\"error\":\"not_implemented\"}";
    }

    /**
     * @brief Опросить события, генерируемые плагином.
     * Вызывается транспортным мостом после каждого тика.
     * Пример: DetectArucoPlugin возвращает событие при обнаружении маркера.
     * @return Список событий (очищается после возврата).
     */
    virtual std::vector<s2::TransportEvent> poll_events() { return {}; }

    // ─── Методы подписки на внешние топики (для визуализационных плагинов) ──

    /**
     * @brief Топики, на которые хочет подписаться плагин (не команды, а данные).
     * Транспортный адаптер подпишется на эти топики и вызовет handle_subscription().
     * Пример: PathDisplayPlugin возвращает {"/plan"} для nav_msgs/Path.
     *
     * В отличие от command_topics(), эти подписки не проходят через handle_input(),
     * а вызывают handle_subscription() напрямую с сырым JSON.
     */
    virtual std::vector<std::string> subscribe_topics() const { return {}; }

    /**
     * @brief ROS2 тип сообщения для топиков из subscribe_topics().
     * Транспортный адаптер использует это значение для выбора нужного десериализатора.
     * Переопределяется плагином, если он подписывается не на nav_msgs/Path.
     * Пример: TopicDisplayPlugin возвращает "std_msgs/String".
     */
    virtual std::string subscription_msg_type() const { return "nav_msgs/Path"; }

    /**
     * @brief Обработать входящее сообщение из подписанного топика.
     * @param topic  Имя топика из subscribe_topics()
     * @param msg_json  JSON-представление сообщения
     */
    virtual void handle_subscription(const std::string& topic,
                                     const std::string& msg_json)
    {
        (void)topic;
        (void)msg_json;
    }

    // ─── Монтаж сенсора ────────────────────────────────────────────────────

    /**
     * @brief Установить точку монтажа сенсора относительно base_link агента.
     *
     * Вызывается SceneLoader'ом при наличии поля mount: в конфиге плагина.
     * После установки mount_frame() вернёт ненулевой FrameTransform.
     *
     * Пример YAML:
     *   - type: gnss
     *     mount: {x: 0.1, y: 0, z: 0.3}
     */
    void set_mount_pose(const Pose3D& pose) { mount_pose_ = pose; }

    /**
     * @brief Получить FrameTransform для точки монтажа.
     *
     * Возвращает FrameTransform если set_mount_pose() был вызван с ненулевой позой.
     * child_frame формируется автоматически:
     *   - sensor_name пустой: "<type>_link"
     *   - sensor_name "left":  "left_<type>_link"
     *
     * SimTransportBridge вызывает этот метод при инициализации
     * для регистрации статических трансформов.
     *
     * @return FrameTransform или nullopt если монтаж не задан
     */
    std::optional<s2::FrameTransform> mount_frame() const
    {
        if (mount_pose_ == Pose3D{})
            return std::nullopt;

        s2::FrameTransform ft;
        ft.parent_frame = "base_link";
        ft.child_frame  = sensor_name_.empty()
            ? (type() + "_link")
            : (sensor_name_ + "_" + type() + "_link");
        ft.relative_pose = mount_pose_;
        return ft;
    }

private:
    std::string sensor_name_;  ///< Пользовательское имя экземпляра ("left", "rear", ...)
    Pose3D      mount_pose_;   ///< Смещение сенсора от base_link (нули = не задан)
    std::string output_topic_; ///< Переопределённое имя выходного топика (пустой = конвенция)
    double      base_rate_hz_{0.0}; ///< Переопределённая частота публикации (0 = default_publish_rate_hz)
};

/**
 * @brief Фабричная функция для создания плагина по типу.
 */
using PluginFactoryFn = std::function<std::unique_ptr<IAgentPlugin>(
    const std::string& type, const YAML::Node& node)>;

} // namespace plugins
} // namespace s2