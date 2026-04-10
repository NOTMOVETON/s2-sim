#pragma once

/**
 * @file ros2_transport_adapter.hpp
 * ROS2 реализация ITransportAdapter.
 *
 * Создаёт per-domain rclcpp::Node с:
 *  - tf2_ros::TransformBroadcaster для odom→base_link
 *  - tf2_ros::StaticTransformBroadcaster для earth→map, map→odom
 *  - Publishers: /gnss/fix, /imu/data, /odom
 *  - Subscribers: /cmd_vel и другие из register_input_topic()
 *  - Service servers: из register_service()
 *  - 1 Hz таймер для earth→map static TF
 */

#include <s2/transport_adapter.hpp>

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <thread>
#include <mutex>

#ifdef S2_WITH_ROS2
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <sensor_msgs/msg/nav_sat_fix.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <nav_msgs/msg/path.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <std_msgs/msg/string.hpp>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2_ros/static_transform_broadcaster.h>
#include <geometry_msgs/msg/transform_stamped.hpp>
#endif

#ifdef S2_WITH_S2_MSGS
#include <s2_msgs/srv/plugin_call.hpp>
#endif

namespace s2
{

/**
 * @brief ROS2 транспортный адаптер.
 *
 * Реализует ITransportAdapter поверх rclcpp.
 * Каждый domain_id получает изолированный rclcpp::Context + Node.
 *
 * В stub-режиме (без S2_WITH_ROS2) все методы — no-op.
 */
class Ros2TransportAdapter : public ITransportAdapter
{
public:
    Ros2TransportAdapter();
    ~Ros2TransportAdapter() override;

    // ITransportAdapter
    void start() override;
    void stop()  override;

    void set_geo_origin(const GeoOrigin& origin) override;
    void register_agent(AgentId id, int domain_id,
                        const std::string& name,
                        const Pose3D& initial_pose) override;
    void register_sensor(SensorRegistration reg) override;
    void register_static_transforms(AgentId id, int domain_id,
                                    const std::vector<FrameTransform>& transforms) override;
    void register_subscription(SubscriptionDesc desc) override;
    void register_input_topic(InputTopicDesc desc) override;
    void register_service(ServiceDesc desc) override;
    void publish_agent_frame(const AgentSensorFrame& frame) override;
    void emit_event(const TransportEvent& event) override;

private:
#ifdef S2_WITH_ROS2
    struct NodeInfo
    {
        std::shared_ptr<rclcpp::Context>  context;
        std::shared_ptr<rclcpp::Node>     node;
        std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> executor;

        // Subscribers
        std::vector<rclcpp::SubscriptionBase::SharedPtr> subscriptions;

        // Publishers — сенсоры (ключ: sensor_name, "" = безымянный)
        // topic: /gnss/<name>/fix  или  /gnss/fix  если name пустой
        std::map<std::string, rclcpp::Publisher<sensor_msgs::msg::NavSatFix>::SharedPtr> gnss_pubs;
        std::map<std::string, rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr>       imu_pubs;
        std::map<std::string, rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr>     odom_pubs;

        // TF
        std::shared_ptr<tf2_ros::TransformBroadcaster>       tf_broadcaster;
        std::shared_ptr<tf2_ros::StaticTransformBroadcaster> static_tf_broadcaster;

        // Сервисы
        std::vector<rclcpp::ServiceBase::SharedPtr> services;

        // Event publishers (topic → publisher), создаются лениво
        std::map<std::string,
                 rclcpp::Publisher<std_msgs::msg::String>::SharedPtr> event_pubs;

        // earth→map TF (предвычислен из geo_origin + начальной позы агента)
        geometry_msgs::msg::TransformStamped earth_map_tf;
        rclcpp::TimerBase::SharedPtr         earth_map_timer;

        // Начальная поза агента в map-координатах (сохраняется при register_agent)
        Pose3D initial_pose{};

        // Дополнительные статические TF (fixed joints + sensor mounting)
        // Добавляются через register_static_transforms(), публикуются вместе с earth→map
        std::vector<geometry_msgs::msg::TransformStamped> extra_static_tfs;

        bool initialized{false};  ///< true после первого register_agent
    };

    std::map<int, NodeInfo> domain_nodes_;   // domain_id → NodeInfo
    std::vector<std::thread> spin_threads_;  // по одному на domain
    std::atomic<bool> running_{false};
    std::mutex mutex_;                       // защищает domain_nodes_ при init

    GeoOrigin geo_origin_;
    bool geo_origin_set_{false};

    // Получить или создать NodeInfo для domain_id
    NodeInfo& get_or_create_node(int domain_id);

    // Вычислить earth→map TransformStamped из geo_origin + смещения в ENU (initial_pose)
    geometry_msgs::msg::TransformStamped compute_earth_map_tf(
        const GeoOrigin& origin,
        const Pose3D& initial_pose,
        const std::shared_ptr<rclcpp::Node>& node) const;

    // Установить static TF (earth→map + map→odom) для domain
    void setup_static_tf(NodeInfo& info);
#endif
};

} // namespace s2
