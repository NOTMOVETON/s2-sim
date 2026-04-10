#pragma once

/**
 * @file ros2_transport.hpp
 * ROS2 транспортный плагин для S2.
 *
 * MVP: только подписка на /cmd_vel для каждого агента.
 * Интеграция с SimEngine через handle_plugin_input().
 */

#include <s2/triple_buffer.hpp>
#include <s2/types.hpp>

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <map>
#include <functional>

#ifdef S2_WITH_ROS2
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#endif

namespace s2
{

// Forward declarations
class SimEngine;

/**
 * @brief Команда управления для одного агента.
 */
struct CmdVelCommand {
    AgentId agent_id{0};
    double linear_velocity{0.0};
    double angular_velocity{0.0};
    bool has_command{false};  // false если нули — игнорировать
};

/**
 * @brief Callback, который транспорт вызывает при получении команды.
 */
using CmdVelCallback = std::function<void(const CmdVelCommand& cmd)>;

/**
 * @brief ROS2 транспортный плагин.
 *
 * В MVP режиме:
 *  - Создаёт отдельный rclcpp::Node для каждого domain_id
 *  - Подписывается на /cmd_vel (без префикса) в каждом domain
 *  - При получении команды вызывает установленный callback
 *
 * Без ROS2 (stub режим): просто заглушка, которая ничего не делает.
 */
class ROS2Transport
{
public:
    ROS2Transport();
    ~ROS2Transport();

    /**
     * @brief Установить callback для получения команд cmd_vel.
     */
    void set_cmd_callback(CmdVelCallback cb);

    /**
     * @brief Запустить транспортный поток.
     */
    void start();

    /**
     * @brief Остановить транспортный поток.
     */
    void stop();

    /**
     * @brief Проверить, работает ли транспорт.
     */
    bool is_running() const { return running_.load(); }

    /**
     * @brief Зарегистрировать агента для подписки на cmd_vel.
     * @param agent_id ID агента
     * @param domain_id ROS domain_id для изоляции агента
     */
    void register_agent_cmd_vel(AgentId agent_id, int domain_id);

private:
    CmdVelCallback cmd_callback_;
    std::atomic<bool> running_{false};
    std::vector<std::thread> spin_threads_;  // Один поток на домен

#ifdef S2_WITH_ROS2
    // Информация о node для каждого domain_id
    struct NodeInfo {
        std::shared_ptr<rclcpp::Context> context;
        std::shared_ptr<rclcpp::Node> node;
        std::unique_ptr<rclcpp::executors::SingleThreadedExecutor> executor;
        std::vector<std::shared_ptr<rclcpp::Subscription<geometry_msgs::msg::Twist>>> subscriptions;
    };
    std::map<int, NodeInfo> domain_nodes_;  // domain_id -> NodeInfo
#endif
};

} // namespace s2