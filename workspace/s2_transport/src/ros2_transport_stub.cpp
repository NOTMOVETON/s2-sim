#include <s2/ros2_transport.hpp>
#include <iostream>

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
    std::cout << "[s2_transport] ROS2 transport stub started (no ROS2 support). Build with S2_WITH_ROS2=ON for full support." << std::endl;
    running_ = true;
}

void ROS2Transport::stop()
{
    running_ = false;
    if (spin_thread_.joinable()) {
        spin_thread_.join();
    }
}

void ROS2Transport::spin_thread()
{
    // Stub: nothing to do without ROS2
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ROS2Transport::register_agent_cmd_vel(AgentId agent_id, int domain_id)
{
    // Stub: no-op, ROS2 not available
    std::cout << "[s2_transport] Stub: register_agent_cmd_vel(agent_id=" << agent_id
              << ", domain_id=" << domain_id << ") — no ROS2" << std::endl;
    (void)agent_id;
    (void)domain_id;
}

} // namespace s2