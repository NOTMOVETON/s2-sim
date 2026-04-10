// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from s2_msgs:srv/PluginCall.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "s2_msgs/srv/plugin_call.hpp"


#ifndef S2_MSGS__SRV__DETAIL__PLUGIN_CALL__TRAITS_HPP_
#define S2_MSGS__SRV__DETAIL__PLUGIN_CALL__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "s2_msgs/srv/detail/plugin_call__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

namespace s2_msgs
{

namespace srv
{

inline void to_flow_style_yaml(
  const PluginCall_Request & msg,
  std::ostream & out)
{
  out << "{";
  // member: request_json
  {
    out << "request_json: ";
    rosidl_generator_traits::value_to_yaml(msg.request_json, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const PluginCall_Request & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: request_json
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "request_json: ";
    rosidl_generator_traits::value_to_yaml(msg.request_json, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const PluginCall_Request & msg, bool use_flow_style = false)
{
  std::ostringstream out;
  if (use_flow_style) {
    to_flow_style_yaml(msg, out);
  } else {
    to_block_style_yaml(msg, out);
  }
  return out.str();
}

}  // namespace srv

}  // namespace s2_msgs

namespace rosidl_generator_traits
{

[[deprecated("use s2_msgs::srv::to_block_style_yaml() instead")]]
inline void to_yaml(
  const s2_msgs::srv::PluginCall_Request & msg,
  std::ostream & out, size_t indentation = 0)
{
  s2_msgs::srv::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use s2_msgs::srv::to_yaml() instead")]]
inline std::string to_yaml(const s2_msgs::srv::PluginCall_Request & msg)
{
  return s2_msgs::srv::to_yaml(msg);
}

template<>
inline const char * data_type<s2_msgs::srv::PluginCall_Request>()
{
  return "s2_msgs::srv::PluginCall_Request";
}

template<>
inline const char * name<s2_msgs::srv::PluginCall_Request>()
{
  return "s2_msgs/srv/PluginCall_Request";
}

template<>
struct has_fixed_size<s2_msgs::srv::PluginCall_Request>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<s2_msgs::srv::PluginCall_Request>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<s2_msgs::srv::PluginCall_Request>
  : std::true_type {};

}  // namespace rosidl_generator_traits

namespace s2_msgs
{

namespace srv
{

inline void to_flow_style_yaml(
  const PluginCall_Response & msg,
  std::ostream & out)
{
  out << "{";
  // member: success
  {
    out << "success: ";
    rosidl_generator_traits::value_to_yaml(msg.success, out);
    out << ", ";
  }

  // member: response_json
  {
    out << "response_json: ";
    rosidl_generator_traits::value_to_yaml(msg.response_json, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const PluginCall_Response & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: success
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "success: ";
    rosidl_generator_traits::value_to_yaml(msg.success, out);
    out << "\n";
  }

  // member: response_json
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "response_json: ";
    rosidl_generator_traits::value_to_yaml(msg.response_json, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const PluginCall_Response & msg, bool use_flow_style = false)
{
  std::ostringstream out;
  if (use_flow_style) {
    to_flow_style_yaml(msg, out);
  } else {
    to_block_style_yaml(msg, out);
  }
  return out.str();
}

}  // namespace srv

}  // namespace s2_msgs

namespace rosidl_generator_traits
{

[[deprecated("use s2_msgs::srv::to_block_style_yaml() instead")]]
inline void to_yaml(
  const s2_msgs::srv::PluginCall_Response & msg,
  std::ostream & out, size_t indentation = 0)
{
  s2_msgs::srv::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use s2_msgs::srv::to_yaml() instead")]]
inline std::string to_yaml(const s2_msgs::srv::PluginCall_Response & msg)
{
  return s2_msgs::srv::to_yaml(msg);
}

template<>
inline const char * data_type<s2_msgs::srv::PluginCall_Response>()
{
  return "s2_msgs::srv::PluginCall_Response";
}

template<>
inline const char * name<s2_msgs::srv::PluginCall_Response>()
{
  return "s2_msgs/srv/PluginCall_Response";
}

template<>
struct has_fixed_size<s2_msgs::srv::PluginCall_Response>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<s2_msgs::srv::PluginCall_Response>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<s2_msgs::srv::PluginCall_Response>
  : std::true_type {};

}  // namespace rosidl_generator_traits

// Include directives for member types
// Member 'info'
#include "service_msgs/msg/detail/service_event_info__traits.hpp"

namespace s2_msgs
{

namespace srv
{

inline void to_flow_style_yaml(
  const PluginCall_Event & msg,
  std::ostream & out)
{
  out << "{";
  // member: info
  {
    out << "info: ";
    to_flow_style_yaml(msg.info, out);
    out << ", ";
  }

  // member: request
  {
    if (msg.request.size() == 0) {
      out << "request: []";
    } else {
      out << "request: [";
      size_t pending_items = msg.request.size();
      for (auto item : msg.request) {
        to_flow_style_yaml(item, out);
        if (--pending_items > 0) {
          out << ", ";
        }
      }
      out << "]";
    }
    out << ", ";
  }

  // member: response
  {
    if (msg.response.size() == 0) {
      out << "response: []";
    } else {
      out << "response: [";
      size_t pending_items = msg.response.size();
      for (auto item : msg.response) {
        to_flow_style_yaml(item, out);
        if (--pending_items > 0) {
          out << ", ";
        }
      }
      out << "]";
    }
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const PluginCall_Event & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: info
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "info:\n";
    to_block_style_yaml(msg.info, out, indentation + 2);
  }

  // member: request
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    if (msg.request.size() == 0) {
      out << "request: []\n";
    } else {
      out << "request:\n";
      for (auto item : msg.request) {
        if (indentation > 0) {
          out << std::string(indentation, ' ');
        }
        out << "-\n";
        to_block_style_yaml(item, out, indentation + 2);
      }
    }
  }

  // member: response
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    if (msg.response.size() == 0) {
      out << "response: []\n";
    } else {
      out << "response:\n";
      for (auto item : msg.response) {
        if (indentation > 0) {
          out << std::string(indentation, ' ');
        }
        out << "-\n";
        to_block_style_yaml(item, out, indentation + 2);
      }
    }
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const PluginCall_Event & msg, bool use_flow_style = false)
{
  std::ostringstream out;
  if (use_flow_style) {
    to_flow_style_yaml(msg, out);
  } else {
    to_block_style_yaml(msg, out);
  }
  return out.str();
}

}  // namespace srv

}  // namespace s2_msgs

namespace rosidl_generator_traits
{

[[deprecated("use s2_msgs::srv::to_block_style_yaml() instead")]]
inline void to_yaml(
  const s2_msgs::srv::PluginCall_Event & msg,
  std::ostream & out, size_t indentation = 0)
{
  s2_msgs::srv::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use s2_msgs::srv::to_yaml() instead")]]
inline std::string to_yaml(const s2_msgs::srv::PluginCall_Event & msg)
{
  return s2_msgs::srv::to_yaml(msg);
}

template<>
inline const char * data_type<s2_msgs::srv::PluginCall_Event>()
{
  return "s2_msgs::srv::PluginCall_Event";
}

template<>
inline const char * name<s2_msgs::srv::PluginCall_Event>()
{
  return "s2_msgs/srv/PluginCall_Event";
}

template<>
struct has_fixed_size<s2_msgs::srv::PluginCall_Event>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<s2_msgs::srv::PluginCall_Event>
  : std::integral_constant<bool, has_bounded_size<s2_msgs::srv::PluginCall_Request>::value && has_bounded_size<s2_msgs::srv::PluginCall_Response>::value && has_bounded_size<service_msgs::msg::ServiceEventInfo>::value> {};

template<>
struct is_message<s2_msgs::srv::PluginCall_Event>
  : std::true_type {};

}  // namespace rosidl_generator_traits

namespace rosidl_generator_traits
{

template<>
inline const char * data_type<s2_msgs::srv::PluginCall>()
{
  return "s2_msgs::srv::PluginCall";
}

template<>
inline const char * name<s2_msgs::srv::PluginCall>()
{
  return "s2_msgs/srv/PluginCall";
}

template<>
struct has_fixed_size<s2_msgs::srv::PluginCall>
  : std::integral_constant<
    bool,
    has_fixed_size<s2_msgs::srv::PluginCall_Request>::value &&
    has_fixed_size<s2_msgs::srv::PluginCall_Response>::value
  >
{
};

template<>
struct has_bounded_size<s2_msgs::srv::PluginCall>
  : std::integral_constant<
    bool,
    has_bounded_size<s2_msgs::srv::PluginCall_Request>::value &&
    has_bounded_size<s2_msgs::srv::PluginCall_Response>::value
  >
{
};

template<>
struct is_service<s2_msgs::srv::PluginCall>
  : std::true_type
{
};

template<>
struct is_service_request<s2_msgs::srv::PluginCall_Request>
  : std::true_type
{
};

template<>
struct is_service_response<s2_msgs::srv::PluginCall_Response>
  : std::true_type
{
};

}  // namespace rosidl_generator_traits

#endif  // S2_MSGS__SRV__DETAIL__PLUGIN_CALL__TRAITS_HPP_
