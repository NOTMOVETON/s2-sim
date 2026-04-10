// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from s2_msgs:srv/PluginCall.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "s2_msgs/srv/plugin_call.h"


#ifndef S2_MSGS__SRV__DETAIL__PLUGIN_CALL__STRUCT_H_
#define S2_MSGS__SRV__DETAIL__PLUGIN_CALL__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


// Constants defined in the message

// Include directives for member types
// Member 'request_json'
#include "rosidl_runtime_c/string.h"

/// Struct defined in srv/PluginCall in the package s2_msgs.
typedef struct s2_msgs__srv__PluginCall_Request
{
  rosidl_runtime_c__String request_json;
} s2_msgs__srv__PluginCall_Request;

// Struct for a sequence of s2_msgs__srv__PluginCall_Request.
typedef struct s2_msgs__srv__PluginCall_Request__Sequence
{
  s2_msgs__srv__PluginCall_Request * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} s2_msgs__srv__PluginCall_Request__Sequence;

// Constants defined in the message

// Include directives for member types
// Member 'response_json'
// already included above
// #include "rosidl_runtime_c/string.h"

/// Struct defined in srv/PluginCall in the package s2_msgs.
typedef struct s2_msgs__srv__PluginCall_Response
{
  bool success;
  rosidl_runtime_c__String response_json;
} s2_msgs__srv__PluginCall_Response;

// Struct for a sequence of s2_msgs__srv__PluginCall_Response.
typedef struct s2_msgs__srv__PluginCall_Response__Sequence
{
  s2_msgs__srv__PluginCall_Response * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} s2_msgs__srv__PluginCall_Response__Sequence;

// Constants defined in the message

// Include directives for member types
// Member 'info'
#include "service_msgs/msg/detail/service_event_info__struct.h"

// constants for array fields with an upper bound
// request
enum
{
  s2_msgs__srv__PluginCall_Event__request__MAX_SIZE = 1
};
// response
enum
{
  s2_msgs__srv__PluginCall_Event__response__MAX_SIZE = 1
};

/// Struct defined in srv/PluginCall in the package s2_msgs.
typedef struct s2_msgs__srv__PluginCall_Event
{
  service_msgs__msg__ServiceEventInfo info;
  s2_msgs__srv__PluginCall_Request__Sequence request;
  s2_msgs__srv__PluginCall_Response__Sequence response;
} s2_msgs__srv__PluginCall_Event;

// Struct for a sequence of s2_msgs__srv__PluginCall_Event.
typedef struct s2_msgs__srv__PluginCall_Event__Sequence
{
  s2_msgs__srv__PluginCall_Event * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} s2_msgs__srv__PluginCall_Event__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // S2_MSGS__SRV__DETAIL__PLUGIN_CALL__STRUCT_H_
