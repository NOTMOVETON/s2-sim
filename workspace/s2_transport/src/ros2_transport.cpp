#include <s2/ros2_transport.hpp>
#include <iostream>
#include <chrono>

#ifdef S2_WITH_ROS2
#include <rclcpp/rclcpp.hpp>
#include <rcl/context.h>
#include <geometry_msgs/msg/twist.hpp>
#endif

namespace s2
{

ROS2Transport::ROS2Transport() = default;
ROS2Transport::~ROS2Transport()
{
    stop();
}

void ROS2Transport::set_cmd_callback(CmdVelCallback cb)
{
    cmd_callback_ = std::move(cb);
}

void ROS2Transport::start()
{
    if (running_.load()) {
        return;
    }

    std::cout << "[s2_transport] ROS2 transport starting..." << std::endl;
    running_ = true;

#ifdef S2_WITH_ROS2
    // Запускаем один поток на каждый зарегистрированный домен
    for (auto& [domain_id, info] : domain_nodes_) {
        auto* executor_ptr = info.executor.get();
        int did = domain_id;
        spin_threads_.emplace_back([this, executor_ptr, did]() {
            std::cout << "[s2_transport] Transport thread started for domain " << did << std::endl;
            while (running_.load()) {
                executor_ptr->spin_some(std::chrono::milliseconds(10));
            }
            std::cout << "[s2_transport] Transport thread exiting for domain " << did << std::endl;
        });
    }

    if (spin_threads_.empty()) {
        std::cout << "[s2_transport] No domains registered, transport idle." << std::endl;
    }
#endif
}

void ROS2Transport::stop()
{
    if (!running_.load()) {
        return;
    }

    running_ = false;

    for (auto& t : spin_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    spin_threads_.clear();

#ifdef S2_WITH_ROS2
    // Завершаем каждый изолированный контекст перед удалением
    for (auto& [domain_id, info] : domain_nodes_) {
        if (info.context && info.context->is_valid()) {
            info.context->shutdown("s2_transport stopped");
        }
    }
    domain_nodes_.clear();
#endif

    std::cout << "[s2_transport] ROS2 transport stopped." << std::endl;
}

/**
 * @brief Зарегистрировать агента для подписки на cmd_vel.
 * Создаёт изолированный rclcpp::Context с нужным domain_id, что обеспечивает
 * физическую изоляцию ROS2-доменов в рамках одного процесса.
 */
void ROS2Transport::register_agent_cmd_vel(AgentId agent_id, int domain_id)
{
#ifdef S2_WITH_ROS2
    auto it = domain_nodes_.find(domain_id);
    if (it == domain_nodes_.end()) {
        // Создаём изолированный контекст для данного ROS2-домена
        auto context = std::make_shared<rclcpp::Context>();
        rclcpp::InitOptions init_options;
        init_options.set_domain_id(static_cast<size_t>(domain_id));
        context->init(0, nullptr, init_options);

        // Создаём ноду в этом контексте
        rclcpp::NodeOptions node_options;
        node_options.context(context);
        auto node = std::make_shared<rclcpp::Node>(
            "s2_agent_domain_" + std::to_string(domain_id),
            node_options
        );

        // Создаём executor, привязанный к данному контексту
        rclcpp::ExecutorOptions exec_options;
        exec_options.context = context;
        auto executor = std::make_unique<rclcpp::executors::SingleThreadedExecutor>(exec_options);
        executor->add_node(node);

        domain_nodes_[domain_id] = NodeInfo{context, node, std::move(executor), {}};
        std::cout << "[s2_transport] Created isolated node for domain " << domain_id << std::endl;
    }

    NodeInfo& info = domain_nodes_[domain_id];

    // Подписка на /cmd_vel без префикса в данном домене
    auto subscription = info.node->create_subscription<geometry_msgs::msg::Twist>(
        "/cmd_vel",
        10,
        [this, agent_id](const geometry_msgs::msg::Twist::SharedPtr msg) {
            CmdVelCommand cmd;
            cmd.agent_id = agent_id;
            cmd.linear_velocity = msg->linear.x;
            cmd.angular_velocity = msg->angular.z;
            cmd.has_command = true;
            if (cmd_callback_) {
                cmd_callback_(cmd);
            }
        });

    info.subscriptions.push_back(subscription);
    std::cout << "[s2_transport] Agent " << agent_id << " registered in domain " << domain_id
              << " subscribed to /cmd_vel" << std::endl;
#else
    (void)agent_id;
    (void)domain_id;
#endif
}

} // namespace s2
