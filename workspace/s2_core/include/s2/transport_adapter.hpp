#pragma once

/**
 * @file transport_adapter.hpp
 * Transport-agnostic интерфейс для публикации данных симуляции.
 *
 * Не содержит ROS2-специфичных типов — MQTT, gRPC или другой транспорт
 * может реализовать этот же интерфейс.
 */

#include <s2/types.hpp>
#include <s2/sensor_data.hpp>
#include <s2/geo_origin.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <memory>

namespace s2
{

/**
 * @brief Данные одного сенсорного плагина для публикации.
 *
 * Хранит КОПИИ данных (не указатели на SharedState), что гарантирует корректность
 * при асинхронной передаче и последующем чтении после следующих тиков.
 * Только одно из полей gnss/imu/diff_drive заполнено — соответствует sensor_type.
 */
struct SensorOutput
{
    std::string sensor_type;                       ///< "gnss", "imu", "diff_drive"
    std::string sensor_name;                       ///< "" = безымянный
    std::optional<GnssData>      gnss;             ///< заполнен для sensor_type == "gnss"
    std::optional<ImuData>       imu;              ///< заполнен для sensor_type == "imu"
    std::optional<DiffDriveData> diff_drive;       ///< заполнен для sensor_type == "diff_drive"
};

/**
 * @brief Регистрация сенсорного плагина в адаптере.
 * Вызывается в SimTransportBridge::init() для каждого сенсорного плагина.
 * Адаптер создаёт publisher с именем топика, производным от sensor_type и sensor_name.
 *
 * Конвенция топиков:
 *   gnss  "":      /gnss/fix
 *   gnss  "left":  /gnss/left/fix
 *   imu   "":      /imu/data
 *   imu   "front": /imu/front/data
 *   diff_drive "": /odom
 */
struct SensorRegistration
{
    AgentId     agent_id{0};
    int         domain_id{0};
    std::string sensor_type;      ///< "gnss", "imu", "diff_drive"
    std::string sensor_name;      ///< "" = по умолчанию
    std::string topic_override;   ///< Если непустой — адаптер использует это имя топика напрямую
};

/**
 * @brief Трансформ между двумя именованными фреймами.
 *
 * Используется для передачи трансформов кинематического дерева
 * и точек монтажа сенсоров через транспортный слой.
 *
 * Статические трансформы (fixed joints, sensor mounting) регистрируются
 * один раз через ITransportAdapter::register_static_transforms().
 *
 * Динамические трансформы (revolute/prismatic joints) передаются
 * каждый тик в AgentSensorFrame::dynamic_transforms.
 */
struct FrameTransform
{
    std::string parent_frame;   ///< Имя родительского фрейма, например "base_link"
    std::string child_frame;    ///< Имя дочернего фрейма, например "lidar_front"
    Pose3D      relative_pose;  ///< Поза дочернего фрейма в системе родителя
};

/**
 * @brief Кадр сенсорных данных одного агента для публикации.
 *
 * sensors содержит только плагины, чей seq изменился с момента последней публикации.
 * TF (odom→base_link) публикуется каждый кадр независимо от sensor_rate.
 */
struct AgentSensorFrame
{
    AgentId agent_id{0};
    int     domain_id{0};
    double  sim_time{0.0};

    Pose3D    world_pose;
    Velocity  world_velocity;

    /// Плагины с новыми данными (seq изменился с последней публикации).
    /// Пустой — данных нет (TF всё равно публикуется).
    std::vector<SensorOutput> sensors;

    /// Динамические трансформы кинематического дерева (revolute/prismatic джоинты).
    /// Публикуются каждый кадр вместе с odom→base_link.
    /// Пустой — у агента нет движущихся джоинтов (или нет kinematic_tree).
    std::vector<FrameTransform> dynamic_transforms;
};

/**
 * @brief Описание подписки на внешний топик данных для плагина агента.
 *
 * В отличие от InputTopicDesc (Twist-команды), используется для подписки
 * на произвольные топики данных (например, nav_msgs/Path).
 * Callback вызывает plugin->handle_subscription(topic, msg_json).
 */
struct SubscriptionDesc
{
    std::string topic;        ///< Например "/plan"
    std::string msg_type;     ///< Например "nav_msgs/Path"
    std::string plugin_type;  ///< Тип плагина: "path_display", ...
    AgentId     agent_id{0};
    int         domain_id{0};

    /// Вызывается адаптером при получении сообщения с JSON-представлением
    std::function<void(const std::string& topic, const std::string& msg_json)> callback;
};

/**
 * @brief Описание входного топика команд для плагина агента.
 */
struct InputTopicDesc
{
    std::string topic;        ///< Например "/cmd_vel", "/cmd_vel_mount"
    std::string plugin_type;  ///< Тип плагина: "diff_drive", "joint_vel", ...
    AgentId     agent_id{0};
    int         domain_id{0};

    /// Вызывается адаптером при получении сообщения; возвращает JSON для handle_input
    std::function<void(const std::string& topic_msg_json)> callback;
};

/**
 * @brief Описание сервиса, предоставляемого плагином агента.
 */
struct ServiceDesc
{
    std::string service_name;  ///< ROS2-имя сервиса, например "/grab"
    std::string plugin_type;
    AgentId     agent_id{0};
    int         domain_id{0};
    bool        is_trigger{false};  ///< true → std_srvs/Trigger, false → s2_msgs/PluginCall

    /// Вызывается адаптером при вызове сервиса; возвращает JSON-ответ
    std::function<std::string(const std::string& request_json)> handler;
};

/**
 * @brief Событие, генерируемое плагином (ArUco, zone-trigger и т.д.).
 */
struct TransportEvent
{
    std::string topic;        ///< Например "/aruco/detected"
    std::string payload_json; ///< JSON-данные события
    AgentId     agent_id{0};
    int         domain_id{0};
};

/**
 * @brief Абстрактный интерфейс транспортного адаптера.
 *
 * Реализации: Ros2TransportAdapter, (будущие: MqttTransportAdapter и т.д.)
 * Все методы потокобезопасны (вызываются из sim-потока и transport-потоков).
 */
class ITransportAdapter
{
public:
    virtual ~ITransportAdapter() = default;

    // ─── Lifecycle ────────────────────────────────────────────────────────

    virtual void start() = 0;
    virtual void stop()  = 0;

    // ─── Инициализация ───────────────────────────────────────────────────

    /**
     * @brief Установить географическое начало координат (для earth→map TF).
     * Вызывается до start().
     */
    virtual void set_geo_origin(const GeoOrigin& origin) = 0;

    /**
     * @brief Зарегистрировать агента.
     * Вызывается один раз до start() для каждого агента.
     * @param initial_pose  Начальная поза агента в глобальных (map) координатах.
     *                      Используется для вычисления earth→map TF (так что начало
     *                      координат каждого робота совпадает с его точкой спавна)
     *                      и для публикации odom→base_link относительно старта.
     */
    virtual void register_agent(AgentId id, int domain_id,
                                const std::string& name,
                                const Pose3D& initial_pose) = 0;

    /**
     * @brief Зарегистрировать подписку на внешний топик данных.
     * Адаптер подписывается на topic (тип определяется msg_type) и вызывает
     * callback(topic, msg_json) при получении сообщения.
     */
    virtual void register_subscription(SubscriptionDesc desc) = 0;

    /**
     * @brief Зарегистрировать входной топик команд для плагина агента.
     * Адаптер подписывается на topic и вызывает callback при получении сообщения.
     */
    virtual void register_input_topic(InputTopicDesc desc) = 0;

    /**
     * @brief Зарегистрировать сервис плагина.
     * Адаптер создаёт сервер и вызывает handler при обращении к сервису.
     */
    virtual void register_service(ServiceDesc desc) = 0;

    /**
     * @brief Зарегистрировать сенсорный плагин для публикации.
     * Адаптер создаёт publisher(ы) с именем топика из sensor_type + sensor_name.
     * Вызывается до start(), по одному разу для каждого сенсорного плагина.
     */
    virtual void register_sensor(SensorRegistration reg) = 0;

    /**
     * @brief Зарегистрировать статические трансформы агента.
     *
     * Вызывается один раз при инициализации для каждого агента.
     * Включает:
     *  - Fixed-джоинты кинематического дерева (base_link → arm_link и т.д.)
     *  - Точки монтажа сенсоров (base_link → gnss_link, base_link → imu_link и т.д.)
     *
     * Адаптер публикует их как static TF (не меняются со временем).
     * Если transforms пустой — вызов игнорируется.
     */
    virtual void register_static_transforms(
        AgentId id, int domain_id,
        const std::vector<FrameTransform>& transforms) = 0;

    // ─── Публикация (вызывается из post-tick callback) ────────────────────

    /**
     * @brief Опубликовать сенсорные данные и TF для одного агента.
     * Вызывается из SimTransportBridge::on_post_tick() с частотой transport_rate.
     */
    virtual void publish_agent_frame(const AgentSensorFrame& frame) = 0;

    /**
     * @brief Опубликовать событие от плагина (ArUco, зона и т.д.).
     */
    virtual void emit_event(const TransportEvent& event) = 0;
};

} // namespace s2
