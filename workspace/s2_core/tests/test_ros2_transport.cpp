#include <gtest/gtest.h>

#include <s2/ros2_transport.hpp>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>

#include <chrono>
#include <thread>

/**
 * Тесты для ROS2Transport с изолированными контекстами на домен.
 * Транспорт сам управляет контекстами — глобальный rclcpp::init не нужен.
 */

TEST(ROS2TransportTest, StartStop)
{
    s2::ROS2Transport transport;
    transport.start();
    EXPECT_TRUE(transport.is_running());
    transport.stop();
    EXPECT_FALSE(transport.is_running());
}

TEST(ROS2TransportTest, RegisterAgentAndCallback)
{
    s2::ROS2Transport transport;
    bool callback_called = false;
    s2::CmdVelCommand received_cmd{};

    transport.set_cmd_callback([&](const s2::CmdVelCommand& cmd) {
        callback_called = true;
        received_cmd = cmd;
    });

    const s2::AgentId test_agent_id = 42;
    // Регистрация агента в domain 0 — транспорт создаёт изолированный контекст
    transport.register_agent_cmd_vel(test_agent_id, 0);
    transport.start();

    // Создаём publisher в том же изолированном домене 0
    auto pub_context = std::make_shared<rclcpp::Context>();
    rclcpp::InitOptions pub_init_options;
    pub_init_options.set_domain_id(0);
    pub_context->init(0, nullptr, pub_init_options);

    rclcpp::NodeOptions pub_node_options;
    pub_node_options.context(pub_context);
    auto pub_node = std::make_shared<rclcpp::Node>("test_pub", pub_node_options);
    auto publisher = pub_node->create_publisher<geometry_msgs::msg::Twist>("/cmd_vel", 10);

    // Executor для publisher (нужен для DDS discovery)
    rclcpp::ExecutorOptions pub_exec_options;
    pub_exec_options.context = pub_context;
    auto pub_executor = std::make_shared<rclcpp::executors::SingleThreadedExecutor>(pub_exec_options);
    pub_executor->add_node(pub_node);

    // Ждём discovery между двумя участниками в domain 0
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto msg = std::make_shared<geometry_msgs::msg::Twist>();
    msg->linear.x = 1.5;
    msg->angular.z = 0.3;
    publisher->publish(*msg);

    // Даём время транспортному потоку обработать сообщение
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    transport.stop();
    pub_context->shutdown("test done");

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(received_cmd.agent_id, test_agent_id);
    EXPECT_DOUBLE_EQ(received_cmd.linear_velocity, 1.5);
    EXPECT_DOUBLE_EQ(received_cmd.angular_velocity, 0.3);
    EXPECT_TRUE(received_cmd.has_command);
}
