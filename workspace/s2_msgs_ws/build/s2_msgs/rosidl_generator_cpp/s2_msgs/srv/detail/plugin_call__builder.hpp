// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from s2_msgs:srv/PluginCall.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "s2_msgs/srv/plugin_call.hpp"


#ifndef S2_MSGS__SRV__DETAIL__PLUGIN_CALL__BUILDER_HPP_
#define S2_MSGS__SRV__DETAIL__PLUGIN_CALL__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "s2_msgs/srv/detail/plugin_call__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace s2_msgs
{

namespace srv
{

namespace builder
{

class Init_PluginCall_Request_request_json
{
public:
  Init_PluginCall_Request_request_json()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  ::s2_msgs::srv::PluginCall_Request request_json(::s2_msgs::srv::PluginCall_Request::_request_json_type arg)
  {
    msg_.request_json = std::move(arg);
    return std::move(msg_);
  }

private:
  ::s2_msgs::srv::PluginCall_Request msg_;
};

}  // namespace builder

}  // namespace srv

template<typename MessageType>
auto build();

template<>
inline
auto build<::s2_msgs::srv::PluginCall_Request>()
{
  return s2_msgs::srv::builder::Init_PluginCall_Request_request_json();
}

}  // namespace s2_msgs


namespace s2_msgs
{

namespace srv
{

namespace builder
{

class Init_PluginCall_Response_response_json
{
public:
  explicit Init_PluginCall_Response_response_json(::s2_msgs::srv::PluginCall_Response & msg)
  : msg_(msg)
  {}
  ::s2_msgs::srv::PluginCall_Response response_json(::s2_msgs::srv::PluginCall_Response::_response_json_type arg)
  {
    msg_.response_json = std::move(arg);
    return std::move(msg_);
  }

private:
  ::s2_msgs::srv::PluginCall_Response msg_;
};

class Init_PluginCall_Response_success
{
public:
  Init_PluginCall_Response_success()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_PluginCall_Response_response_json success(::s2_msgs::srv::PluginCall_Response::_success_type arg)
  {
    msg_.success = std::move(arg);
    return Init_PluginCall_Response_response_json(msg_);
  }

private:
  ::s2_msgs::srv::PluginCall_Response msg_;
};

}  // namespace builder

}  // namespace srv

template<typename MessageType>
auto build();

template<>
inline
auto build<::s2_msgs::srv::PluginCall_Response>()
{
  return s2_msgs::srv::builder::Init_PluginCall_Response_success();
}

}  // namespace s2_msgs


namespace s2_msgs
{

namespace srv
{

namespace builder
{

class Init_PluginCall_Event_response
{
public:
  explicit Init_PluginCall_Event_response(::s2_msgs::srv::PluginCall_Event & msg)
  : msg_(msg)
  {}
  ::s2_msgs::srv::PluginCall_Event response(::s2_msgs::srv::PluginCall_Event::_response_type arg)
  {
    msg_.response = std::move(arg);
    return std::move(msg_);
  }

private:
  ::s2_msgs::srv::PluginCall_Event msg_;
};

class Init_PluginCall_Event_request
{
public:
  explicit Init_PluginCall_Event_request(::s2_msgs::srv::PluginCall_Event & msg)
  : msg_(msg)
  {}
  Init_PluginCall_Event_response request(::s2_msgs::srv::PluginCall_Event::_request_type arg)
  {
    msg_.request = std::move(arg);
    return Init_PluginCall_Event_response(msg_);
  }

private:
  ::s2_msgs::srv::PluginCall_Event msg_;
};

class Init_PluginCall_Event_info
{
public:
  Init_PluginCall_Event_info()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_PluginCall_Event_request info(::s2_msgs::srv::PluginCall_Event::_info_type arg)
  {
    msg_.info = std::move(arg);
    return Init_PluginCall_Event_request(msg_);
  }

private:
  ::s2_msgs::srv::PluginCall_Event msg_;
};

}  // namespace builder

}  // namespace srv

template<typename MessageType>
auto build();

template<>
inline
auto build<::s2_msgs::srv::PluginCall_Event>()
{
  return s2_msgs::srv::builder::Init_PluginCall_Event_info();
}

}  // namespace s2_msgs

#endif  // S2_MSGS__SRV__DETAIL__PLUGIN_CALL__BUILDER_HPP_
