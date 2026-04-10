// generated from rosidl_typesupport_introspection_c/resource/idl__type_support.c.em
// with input from s2_msgs:srv/PluginCall.idl
// generated code does not contain a copyright notice

#include <stddef.h>
#include "s2_msgs/srv/detail/plugin_call__rosidl_typesupport_introspection_c.h"
#include "s2_msgs/msg/rosidl_typesupport_introspection_c__visibility_control.h"
#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"
#include "s2_msgs/srv/detail/plugin_call__functions.h"
#include "s2_msgs/srv/detail/plugin_call__struct.h"


// Include directives for member types
// Member `request_json`
#include "rosidl_runtime_c/string_functions.h"

#ifdef __cplusplus
extern "C"
{
#endif

void s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_init_function(
  void * message_memory, enum rosidl_runtime_c__message_initialization _init)
{
  // TODO(karsten1987): initializers are not yet implemented for typesupport c
  // see https://github.com/ros2/ros2/issues/397
  (void) _init;
  s2_msgs__srv__PluginCall_Request__init(message_memory);
}

void s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_fini_function(void * message_memory)
{
  s2_msgs__srv__PluginCall_Request__fini(message_memory);
}

static rosidl_typesupport_introspection_c__MessageMember s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_message_member_array[1] = {
  {
    "request_json",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_STRING,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(s2_msgs__srv__PluginCall_Request, request_json),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  }
};

static const rosidl_typesupport_introspection_c__MessageMembers s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_message_members = {
  "s2_msgs__srv",  // message namespace
  "PluginCall_Request",  // message name
  1,  // number of fields
  sizeof(s2_msgs__srv__PluginCall_Request),
  false,  // has_any_key_member_
  s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_message_member_array,  // message members
  s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_init_function,  // function to initialize message memory (memory has to be allocated)
  s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_fini_function  // function to terminate message instance (will not free memory)
};

// this is not const since it must be initialized on first access
// since C does not allow non-integral compile-time constants
static rosidl_message_type_support_t s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_message_type_support_handle = {
  0,
  &s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_message_members,
  get_message_typesupport_handle_function,
  &s2_msgs__srv__PluginCall_Request__get_type_hash,
  &s2_msgs__srv__PluginCall_Request__get_type_description,
  &s2_msgs__srv__PluginCall_Request__get_type_description_sources,
};

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_s2_msgs
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, s2_msgs, srv, PluginCall_Request)() {
  if (!s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_message_type_support_handle.typesupport_identifier) {
    s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_message_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  return &s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_message_type_support_handle;
}
#ifdef __cplusplus
}
#endif

// already included above
// #include <stddef.h>
// already included above
// #include "s2_msgs/srv/detail/plugin_call__rosidl_typesupport_introspection_c.h"
// already included above
// #include "s2_msgs/msg/rosidl_typesupport_introspection_c__visibility_control.h"
// already included above
// #include "rosidl_typesupport_introspection_c/field_types.h"
// already included above
// #include "rosidl_typesupport_introspection_c/identifier.h"
// already included above
// #include "rosidl_typesupport_introspection_c/message_introspection.h"
// already included above
// #include "s2_msgs/srv/detail/plugin_call__functions.h"
// already included above
// #include "s2_msgs/srv/detail/plugin_call__struct.h"


// Include directives for member types
// Member `response_json`
// already included above
// #include "rosidl_runtime_c/string_functions.h"

#ifdef __cplusplus
extern "C"
{
#endif

void s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_init_function(
  void * message_memory, enum rosidl_runtime_c__message_initialization _init)
{
  // TODO(karsten1987): initializers are not yet implemented for typesupport c
  // see https://github.com/ros2/ros2/issues/397
  (void) _init;
  s2_msgs__srv__PluginCall_Response__init(message_memory);
}

void s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_fini_function(void * message_memory)
{
  s2_msgs__srv__PluginCall_Response__fini(message_memory);
}

static rosidl_typesupport_introspection_c__MessageMember s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_message_member_array[2] = {
  {
    "success",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_BOOLEAN,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(s2_msgs__srv__PluginCall_Response, success),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "response_json",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_STRING,  // type
    0,  // upper bound of string
    NULL,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(s2_msgs__srv__PluginCall_Response, response_json),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  }
};

static const rosidl_typesupport_introspection_c__MessageMembers s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_message_members = {
  "s2_msgs__srv",  // message namespace
  "PluginCall_Response",  // message name
  2,  // number of fields
  sizeof(s2_msgs__srv__PluginCall_Response),
  false,  // has_any_key_member_
  s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_message_member_array,  // message members
  s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_init_function,  // function to initialize message memory (memory has to be allocated)
  s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_fini_function  // function to terminate message instance (will not free memory)
};

// this is not const since it must be initialized on first access
// since C does not allow non-integral compile-time constants
static rosidl_message_type_support_t s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_message_type_support_handle = {
  0,
  &s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_message_members,
  get_message_typesupport_handle_function,
  &s2_msgs__srv__PluginCall_Response__get_type_hash,
  &s2_msgs__srv__PluginCall_Response__get_type_description,
  &s2_msgs__srv__PluginCall_Response__get_type_description_sources,
};

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_s2_msgs
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, s2_msgs, srv, PluginCall_Response)() {
  if (!s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_message_type_support_handle.typesupport_identifier) {
    s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_message_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  return &s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_message_type_support_handle;
}
#ifdef __cplusplus
}
#endif

// already included above
// #include <stddef.h>
// already included above
// #include "s2_msgs/srv/detail/plugin_call__rosidl_typesupport_introspection_c.h"
// already included above
// #include "s2_msgs/msg/rosidl_typesupport_introspection_c__visibility_control.h"
// already included above
// #include "rosidl_typesupport_introspection_c/field_types.h"
// already included above
// #include "rosidl_typesupport_introspection_c/identifier.h"
// already included above
// #include "rosidl_typesupport_introspection_c/message_introspection.h"
// already included above
// #include "s2_msgs/srv/detail/plugin_call__functions.h"
// already included above
// #include "s2_msgs/srv/detail/plugin_call__struct.h"


// Include directives for member types
// Member `info`
#include "service_msgs/msg/service_event_info.h"
// Member `info`
#include "service_msgs/msg/detail/service_event_info__rosidl_typesupport_introspection_c.h"
// Member `request`
// Member `response`
#include "s2_msgs/srv/plugin_call.h"
// Member `request`
// Member `response`
// already included above
// #include "s2_msgs/srv/detail/plugin_call__rosidl_typesupport_introspection_c.h"

#ifdef __cplusplus
extern "C"
{
#endif

void s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_init_function(
  void * message_memory, enum rosidl_runtime_c__message_initialization _init)
{
  // TODO(karsten1987): initializers are not yet implemented for typesupport c
  // see https://github.com/ros2/ros2/issues/397
  (void) _init;
  s2_msgs__srv__PluginCall_Event__init(message_memory);
}

void s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_fini_function(void * message_memory)
{
  s2_msgs__srv__PluginCall_Event__fini(message_memory);
}

size_t s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__size_function__PluginCall_Event__request(
  const void * untyped_member)
{
  const s2_msgs__srv__PluginCall_Request__Sequence * member =
    (const s2_msgs__srv__PluginCall_Request__Sequence *)(untyped_member);
  return member->size;
}

const void * s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__get_const_function__PluginCall_Event__request(
  const void * untyped_member, size_t index)
{
  const s2_msgs__srv__PluginCall_Request__Sequence * member =
    (const s2_msgs__srv__PluginCall_Request__Sequence *)(untyped_member);
  return &member->data[index];
}

void * s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__get_function__PluginCall_Event__request(
  void * untyped_member, size_t index)
{
  s2_msgs__srv__PluginCall_Request__Sequence * member =
    (s2_msgs__srv__PluginCall_Request__Sequence *)(untyped_member);
  return &member->data[index];
}

void s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__fetch_function__PluginCall_Event__request(
  const void * untyped_member, size_t index, void * untyped_value)
{
  const s2_msgs__srv__PluginCall_Request * item =
    ((const s2_msgs__srv__PluginCall_Request *)
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__get_const_function__PluginCall_Event__request(untyped_member, index));
  s2_msgs__srv__PluginCall_Request * value =
    (s2_msgs__srv__PluginCall_Request *)(untyped_value);
  *value = *item;
}

void s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__assign_function__PluginCall_Event__request(
  void * untyped_member, size_t index, const void * untyped_value)
{
  s2_msgs__srv__PluginCall_Request * item =
    ((s2_msgs__srv__PluginCall_Request *)
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__get_function__PluginCall_Event__request(untyped_member, index));
  const s2_msgs__srv__PluginCall_Request * value =
    (const s2_msgs__srv__PluginCall_Request *)(untyped_value);
  *item = *value;
}

bool s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__resize_function__PluginCall_Event__request(
  void * untyped_member, size_t size)
{
  s2_msgs__srv__PluginCall_Request__Sequence * member =
    (s2_msgs__srv__PluginCall_Request__Sequence *)(untyped_member);
  s2_msgs__srv__PluginCall_Request__Sequence__fini(member);
  return s2_msgs__srv__PluginCall_Request__Sequence__init(member, size);
}

size_t s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__size_function__PluginCall_Event__response(
  const void * untyped_member)
{
  const s2_msgs__srv__PluginCall_Response__Sequence * member =
    (const s2_msgs__srv__PluginCall_Response__Sequence *)(untyped_member);
  return member->size;
}

const void * s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__get_const_function__PluginCall_Event__response(
  const void * untyped_member, size_t index)
{
  const s2_msgs__srv__PluginCall_Response__Sequence * member =
    (const s2_msgs__srv__PluginCall_Response__Sequence *)(untyped_member);
  return &member->data[index];
}

void * s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__get_function__PluginCall_Event__response(
  void * untyped_member, size_t index)
{
  s2_msgs__srv__PluginCall_Response__Sequence * member =
    (s2_msgs__srv__PluginCall_Response__Sequence *)(untyped_member);
  return &member->data[index];
}

void s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__fetch_function__PluginCall_Event__response(
  const void * untyped_member, size_t index, void * untyped_value)
{
  const s2_msgs__srv__PluginCall_Response * item =
    ((const s2_msgs__srv__PluginCall_Response *)
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__get_const_function__PluginCall_Event__response(untyped_member, index));
  s2_msgs__srv__PluginCall_Response * value =
    (s2_msgs__srv__PluginCall_Response *)(untyped_value);
  *value = *item;
}

void s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__assign_function__PluginCall_Event__response(
  void * untyped_member, size_t index, const void * untyped_value)
{
  s2_msgs__srv__PluginCall_Response * item =
    ((s2_msgs__srv__PluginCall_Response *)
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__get_function__PluginCall_Event__response(untyped_member, index));
  const s2_msgs__srv__PluginCall_Response * value =
    (const s2_msgs__srv__PluginCall_Response *)(untyped_value);
  *item = *value;
}

bool s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__resize_function__PluginCall_Event__response(
  void * untyped_member, size_t size)
{
  s2_msgs__srv__PluginCall_Response__Sequence * member =
    (s2_msgs__srv__PluginCall_Response__Sequence *)(untyped_member);
  s2_msgs__srv__PluginCall_Response__Sequence__fini(member);
  return s2_msgs__srv__PluginCall_Response__Sequence__init(member, size);
}

static rosidl_typesupport_introspection_c__MessageMember s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_message_member_array[3] = {
  {
    "info",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    NULL,  // members of sub message (initialized later)
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(s2_msgs__srv__PluginCall_Event, info),  // bytes offset in struct
    NULL,  // default value
    NULL,  // size() function pointer
    NULL,  // get_const(index) function pointer
    NULL,  // get(index) function pointer
    NULL,  // fetch(index, &value) function pointer
    NULL,  // assign(index, value) function pointer
    NULL  // resize(index) function pointer
  },
  {
    "request",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    NULL,  // members of sub message (initialized later)
    false,  // is key
    true,  // is array
    1,  // array size
    true,  // is upper bound
    offsetof(s2_msgs__srv__PluginCall_Event, request),  // bytes offset in struct
    NULL,  // default value
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__size_function__PluginCall_Event__request,  // size() function pointer
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__get_const_function__PluginCall_Event__request,  // get_const(index) function pointer
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__get_function__PluginCall_Event__request,  // get(index) function pointer
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__fetch_function__PluginCall_Event__request,  // fetch(index, &value) function pointer
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__assign_function__PluginCall_Event__request,  // assign(index, value) function pointer
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__resize_function__PluginCall_Event__request  // resize(index) function pointer
  },
  {
    "response",  // name
    rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    NULL,  // members of sub message (initialized later)
    false,  // is key
    true,  // is array
    1,  // array size
    true,  // is upper bound
    offsetof(s2_msgs__srv__PluginCall_Event, response),  // bytes offset in struct
    NULL,  // default value
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__size_function__PluginCall_Event__response,  // size() function pointer
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__get_const_function__PluginCall_Event__response,  // get_const(index) function pointer
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__get_function__PluginCall_Event__response,  // get(index) function pointer
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__fetch_function__PluginCall_Event__response,  // fetch(index, &value) function pointer
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__assign_function__PluginCall_Event__response,  // assign(index, value) function pointer
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__resize_function__PluginCall_Event__response  // resize(index) function pointer
  }
};

static const rosidl_typesupport_introspection_c__MessageMembers s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_message_members = {
  "s2_msgs__srv",  // message namespace
  "PluginCall_Event",  // message name
  3,  // number of fields
  sizeof(s2_msgs__srv__PluginCall_Event),
  false,  // has_any_key_member_
  s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_message_member_array,  // message members
  s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_init_function,  // function to initialize message memory (memory has to be allocated)
  s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_fini_function  // function to terminate message instance (will not free memory)
};

// this is not const since it must be initialized on first access
// since C does not allow non-integral compile-time constants
static rosidl_message_type_support_t s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_message_type_support_handle = {
  0,
  &s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_message_members,
  get_message_typesupport_handle_function,
  &s2_msgs__srv__PluginCall_Event__get_type_hash,
  &s2_msgs__srv__PluginCall_Event__get_type_description,
  &s2_msgs__srv__PluginCall_Event__get_type_description_sources,
};

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_s2_msgs
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, s2_msgs, srv, PluginCall_Event)() {
  s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_message_member_array[0].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, service_msgs, msg, ServiceEventInfo)();
  s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_message_member_array[1].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, s2_msgs, srv, PluginCall_Request)();
  s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_message_member_array[2].members_ =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, s2_msgs, srv, PluginCall_Response)();
  if (!s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_message_type_support_handle.typesupport_identifier) {
    s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_message_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  return &s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_message_type_support_handle;
}
#ifdef __cplusplus
}
#endif

#include "rosidl_runtime_c/service_type_support_struct.h"
// already included above
// #include "s2_msgs/msg/rosidl_typesupport_introspection_c__visibility_control.h"
// already included above
// #include "s2_msgs/srv/detail/plugin_call__rosidl_typesupport_introspection_c.h"
// already included above
// #include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/service_introspection.h"

// this is intentionally not const to allow initialization later to prevent an initialization race
static rosidl_typesupport_introspection_c__ServiceMembers s2_msgs__srv__detail__plugin_call__rosidl_typesupport_introspection_c__PluginCall_service_members = {
  "s2_msgs__srv",  // service namespace
  "PluginCall",  // service name
  // the following fields are initialized below on first access
  NULL,  // request message
  // s2_msgs__srv__detail__plugin_call__rosidl_typesupport_introspection_c__PluginCall_Request_message_type_support_handle,
  NULL,  // response message
  // s2_msgs__srv__detail__plugin_call__rosidl_typesupport_introspection_c__PluginCall_Response_message_type_support_handle
  NULL  // event_message
  // s2_msgs__srv__detail__plugin_call__rosidl_typesupport_introspection_c__PluginCall_Response_message_type_support_handle
};


static rosidl_service_type_support_t s2_msgs__srv__detail__plugin_call__rosidl_typesupport_introspection_c__PluginCall_service_type_support_handle = {
  0,
  &s2_msgs__srv__detail__plugin_call__rosidl_typesupport_introspection_c__PluginCall_service_members,
  get_service_typesupport_handle_function,
  &s2_msgs__srv__PluginCall_Request__rosidl_typesupport_introspection_c__PluginCall_Request_message_type_support_handle,
  &s2_msgs__srv__PluginCall_Response__rosidl_typesupport_introspection_c__PluginCall_Response_message_type_support_handle,
  &s2_msgs__srv__PluginCall_Event__rosidl_typesupport_introspection_c__PluginCall_Event_message_type_support_handle,
  ROSIDL_TYPESUPPORT_INTERFACE__SERVICE_CREATE_EVENT_MESSAGE_SYMBOL_NAME(
    rosidl_typesupport_c,
    s2_msgs,
    srv,
    PluginCall
  ),
  ROSIDL_TYPESUPPORT_INTERFACE__SERVICE_DESTROY_EVENT_MESSAGE_SYMBOL_NAME(
    rosidl_typesupport_c,
    s2_msgs,
    srv,
    PluginCall
  ),
  &s2_msgs__srv__PluginCall__get_type_hash,
  &s2_msgs__srv__PluginCall__get_type_description,
  &s2_msgs__srv__PluginCall__get_type_description_sources,
};

// Forward declaration of message type support functions for service members
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, s2_msgs, srv, PluginCall_Request)(void);

const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, s2_msgs, srv, PluginCall_Response)(void);

const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, s2_msgs, srv, PluginCall_Event)(void);

ROSIDL_TYPESUPPORT_INTROSPECTION_C_EXPORT_s2_msgs
const rosidl_service_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__SERVICE_SYMBOL_NAME(rosidl_typesupport_introspection_c, s2_msgs, srv, PluginCall)(void) {
  if (!s2_msgs__srv__detail__plugin_call__rosidl_typesupport_introspection_c__PluginCall_service_type_support_handle.typesupport_identifier) {
    s2_msgs__srv__detail__plugin_call__rosidl_typesupport_introspection_c__PluginCall_service_type_support_handle.typesupport_identifier =
      rosidl_typesupport_introspection_c__identifier;
  }
  rosidl_typesupport_introspection_c__ServiceMembers * service_members =
    (rosidl_typesupport_introspection_c__ServiceMembers *)s2_msgs__srv__detail__plugin_call__rosidl_typesupport_introspection_c__PluginCall_service_type_support_handle.data;

  if (!service_members->request_members_) {
    service_members->request_members_ =
      (const rosidl_typesupport_introspection_c__MessageMembers *)
      ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, s2_msgs, srv, PluginCall_Request)()->data;
  }
  if (!service_members->response_members_) {
    service_members->response_members_ =
      (const rosidl_typesupport_introspection_c__MessageMembers *)
      ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, s2_msgs, srv, PluginCall_Response)()->data;
  }
  if (!service_members->event_members_) {
    service_members->event_members_ =
      (const rosidl_typesupport_introspection_c__MessageMembers *)
      ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_c, s2_msgs, srv, PluginCall_Event)()->data;
  }

  return &s2_msgs__srv__detail__plugin_call__rosidl_typesupport_introspection_c__PluginCall_service_type_support_handle;
}
