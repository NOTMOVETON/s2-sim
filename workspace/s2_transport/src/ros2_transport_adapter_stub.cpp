/**
 * @file ros2_transport_adapter_stub.cpp
 * Заглушка Ros2TransportAdapter для сборки без ROS2 (-DS2_WITH_ROS2=OFF).
 *
 * Все методы — no-op. Позволяет коду main.cpp компилироваться без ROS2.
 */

#ifndef S2_WITH_ROS2

#include <s2/ros2_transport_adapter.hpp>

namespace s2
{

Ros2TransportAdapter::Ros2TransportAdapter()  = default;
Ros2TransportAdapter::~Ros2TransportAdapter() = default;

void Ros2TransportAdapter::start()                                    {}
void Ros2TransportAdapter::stop()                                     {}
void Ros2TransportAdapter::set_geo_origin(const GeoOrigin&)          {}
void Ros2TransportAdapter::register_agent(AgentId, int,
                                          const std::string&,
                                          const Pose3D&)              {}
void Ros2TransportAdapter::register_sensor(SensorRegistration)        {}
void Ros2TransportAdapter::register_static_transforms(AgentId, int,
                                    const std::vector<FrameTransform>&){}
void Ros2TransportAdapter::register_subscription(SubscriptionDesc)     {}
void Ros2TransportAdapter::register_input_topic(InputTopicDesc)       {}
void Ros2TransportAdapter::register_service(ServiceDesc)              {}
void Ros2TransportAdapter::publish_agent_frame(const AgentSensorFrame&){}
void Ros2TransportAdapter::emit_event(const TransportEvent&)          {}

} // namespace s2

#endif // !S2_WITH_ROS2
