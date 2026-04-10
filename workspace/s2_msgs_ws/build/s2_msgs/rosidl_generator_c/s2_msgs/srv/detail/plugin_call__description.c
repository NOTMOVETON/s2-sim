// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from s2_msgs:srv/PluginCall.idl
// generated code does not contain a copyright notice

#include "s2_msgs/srv/detail/plugin_call__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_s2_msgs
const rosidl_type_hash_t *
s2_msgs__srv__PluginCall__get_type_hash(
  const rosidl_service_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x60, 0x2f, 0xa4, 0xae, 0x27, 0x84, 0x2a, 0x87,
      0x29, 0x74, 0x0e, 0x9c, 0x3f, 0xe9, 0xdb, 0x21,
      0x63, 0x07, 0x8e, 0x1a, 0x5b, 0xf1, 0xc0, 0x85,
      0xef, 0x1f, 0x8b, 0x4a, 0xcb, 0x06, 0x48, 0xa4,
    }};
  return &hash;
}

ROSIDL_GENERATOR_C_PUBLIC_s2_msgs
const rosidl_type_hash_t *
s2_msgs__srv__PluginCall_Request__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x6b, 0x0b, 0xef, 0x79, 0xec, 0x75, 0x30, 0xc1,
      0xe1, 0x24, 0x64, 0xc6, 0x88, 0x24, 0x85, 0x7b,
      0x38, 0xa8, 0x5f, 0x9a, 0xc8, 0x03, 0x36, 0x13,
      0x5f, 0x6e, 0x2a, 0x20, 0xfd, 0x6b, 0xbf, 0xa1,
    }};
  return &hash;
}

ROSIDL_GENERATOR_C_PUBLIC_s2_msgs
const rosidl_type_hash_t *
s2_msgs__srv__PluginCall_Response__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x13, 0x1d, 0x77, 0xcc, 0xb4, 0xb2, 0xd6, 0xb0,
      0x2b, 0xcc, 0x22, 0x4b, 0x68, 0xea, 0xa3, 0xf5,
      0x87, 0xbe, 0x59, 0x83, 0xac, 0xcd, 0x75, 0xd7,
      0x7c, 0x4d, 0x04, 0x58, 0xcb, 0xef, 0x9d, 0x5a,
    }};
  return &hash;
}

ROSIDL_GENERATOR_C_PUBLIC_s2_msgs
const rosidl_type_hash_t *
s2_msgs__srv__PluginCall_Event__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0xa6, 0x4e, 0x6b, 0x02, 0xfa, 0xdd, 0x11, 0xdd,
      0x1f, 0x1f, 0x33, 0x41, 0xdf, 0xa3, 0xd4, 0xdb,
      0xae, 0x82, 0x9a, 0xde, 0x83, 0xa8, 0x33, 0x17,
      0xd8, 0x97, 0xfb, 0xbe, 0x84, 0x87, 0x19, 0x4b,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types
#include "service_msgs/msg/detail/service_event_info__functions.h"
#include "builtin_interfaces/msg/detail/time__functions.h"

// Hashes for external referenced types
#ifndef NDEBUG
static const rosidl_type_hash_t builtin_interfaces__msg__Time__EXPECTED_HASH = {1, {
    0xb1, 0x06, 0x23, 0x5e, 0x25, 0xa4, 0xc5, 0xed,
    0x35, 0x09, 0x8a, 0xa0, 0xa6, 0x1a, 0x3e, 0xe9,
    0xc9, 0xb1, 0x8d, 0x19, 0x7f, 0x39, 0x8b, 0x0e,
    0x42, 0x06, 0xce, 0xa9, 0xac, 0xf9, 0xc1, 0x97,
  }};
static const rosidl_type_hash_t service_msgs__msg__ServiceEventInfo__EXPECTED_HASH = {1, {
    0x41, 0xbc, 0xbb, 0xe0, 0x7a, 0x75, 0xc9, 0xb5,
    0x2b, 0xc9, 0x6b, 0xfd, 0x5c, 0x24, 0xd7, 0xf0,
    0xfc, 0x0a, 0x08, 0xc0, 0xcb, 0x79, 0x21, 0xb3,
    0x37, 0x3c, 0x57, 0x32, 0x34, 0x5a, 0x6f, 0x45,
  }};
#endif

static char s2_msgs__srv__PluginCall__TYPE_NAME[] = "s2_msgs/srv/PluginCall";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char s2_msgs__srv__PluginCall_Event__TYPE_NAME[] = "s2_msgs/srv/PluginCall_Event";
static char s2_msgs__srv__PluginCall_Request__TYPE_NAME[] = "s2_msgs/srv/PluginCall_Request";
static char s2_msgs__srv__PluginCall_Response__TYPE_NAME[] = "s2_msgs/srv/PluginCall_Response";
static char service_msgs__msg__ServiceEventInfo__TYPE_NAME[] = "service_msgs/msg/ServiceEventInfo";

// Define type names, field names, and default values
static char s2_msgs__srv__PluginCall__FIELD_NAME__request_message[] = "request_message";
static char s2_msgs__srv__PluginCall__FIELD_NAME__response_message[] = "response_message";
static char s2_msgs__srv__PluginCall__FIELD_NAME__event_message[] = "event_message";

static rosidl_runtime_c__type_description__Field s2_msgs__srv__PluginCall__FIELDS[] = {
  {
    {s2_msgs__srv__PluginCall__FIELD_NAME__request_message, 15, 15},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {s2_msgs__srv__PluginCall_Request__TYPE_NAME, 30, 30},
    },
    {NULL, 0, 0},
  },
  {
    {s2_msgs__srv__PluginCall__FIELD_NAME__response_message, 16, 16},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {s2_msgs__srv__PluginCall_Response__TYPE_NAME, 31, 31},
    },
    {NULL, 0, 0},
  },
  {
    {s2_msgs__srv__PluginCall__FIELD_NAME__event_message, 13, 13},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {s2_msgs__srv__PluginCall_Event__TYPE_NAME, 28, 28},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription s2_msgs__srv__PluginCall__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {builtin_interfaces__msg__Time__TYPE_NAME, 27, 27},
    {NULL, 0, 0},
  },
  {
    {s2_msgs__srv__PluginCall_Event__TYPE_NAME, 28, 28},
    {NULL, 0, 0},
  },
  {
    {s2_msgs__srv__PluginCall_Request__TYPE_NAME, 30, 30},
    {NULL, 0, 0},
  },
  {
    {s2_msgs__srv__PluginCall_Response__TYPE_NAME, 31, 31},
    {NULL, 0, 0},
  },
  {
    {service_msgs__msg__ServiceEventInfo__TYPE_NAME, 33, 33},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
s2_msgs__srv__PluginCall__get_type_description(
  const rosidl_service_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {s2_msgs__srv__PluginCall__TYPE_NAME, 22, 22},
      {s2_msgs__srv__PluginCall__FIELDS, 3, 3},
    },
    {s2_msgs__srv__PluginCall__REFERENCED_TYPE_DESCRIPTIONS, 5, 5},
  };
  if (!constructed) {
    assert(0 == memcmp(&builtin_interfaces__msg__Time__EXPECTED_HASH, builtin_interfaces__msg__Time__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = builtin_interfaces__msg__Time__get_type_description(NULL)->type_description.fields;
    description.referenced_type_descriptions.data[1].fields = s2_msgs__srv__PluginCall_Event__get_type_description(NULL)->type_description.fields;
    description.referenced_type_descriptions.data[2].fields = s2_msgs__srv__PluginCall_Request__get_type_description(NULL)->type_description.fields;
    description.referenced_type_descriptions.data[3].fields = s2_msgs__srv__PluginCall_Response__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&service_msgs__msg__ServiceEventInfo__EXPECTED_HASH, service_msgs__msg__ServiceEventInfo__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[4].fields = service_msgs__msg__ServiceEventInfo__get_type_description(NULL)->type_description.fields;
    constructed = true;
  }
  return &description;
}
// Define type names, field names, and default values
static char s2_msgs__srv__PluginCall_Request__FIELD_NAME__request_json[] = "request_json";

static rosidl_runtime_c__type_description__Field s2_msgs__srv__PluginCall_Request__FIELDS[] = {
  {
    {s2_msgs__srv__PluginCall_Request__FIELD_NAME__request_json, 12, 12},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_STRING,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
s2_msgs__srv__PluginCall_Request__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {s2_msgs__srv__PluginCall_Request__TYPE_NAME, 30, 30},
      {s2_msgs__srv__PluginCall_Request__FIELDS, 1, 1},
    },
    {NULL, 0, 0},
  };
  if (!constructed) {
    constructed = true;
  }
  return &description;
}
// Define type names, field names, and default values
static char s2_msgs__srv__PluginCall_Response__FIELD_NAME__success[] = "success";
static char s2_msgs__srv__PluginCall_Response__FIELD_NAME__response_json[] = "response_json";

static rosidl_runtime_c__type_description__Field s2_msgs__srv__PluginCall_Response__FIELDS[] = {
  {
    {s2_msgs__srv__PluginCall_Response__FIELD_NAME__success, 7, 7},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_BOOLEAN,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {s2_msgs__srv__PluginCall_Response__FIELD_NAME__response_json, 13, 13},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_STRING,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
s2_msgs__srv__PluginCall_Response__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {s2_msgs__srv__PluginCall_Response__TYPE_NAME, 31, 31},
      {s2_msgs__srv__PluginCall_Response__FIELDS, 2, 2},
    },
    {NULL, 0, 0},
  };
  if (!constructed) {
    constructed = true;
  }
  return &description;
}
// Define type names, field names, and default values
static char s2_msgs__srv__PluginCall_Event__FIELD_NAME__info[] = "info";
static char s2_msgs__srv__PluginCall_Event__FIELD_NAME__request[] = "request";
static char s2_msgs__srv__PluginCall_Event__FIELD_NAME__response[] = "response";

static rosidl_runtime_c__type_description__Field s2_msgs__srv__PluginCall_Event__FIELDS[] = {
  {
    {s2_msgs__srv__PluginCall_Event__FIELD_NAME__info, 4, 4},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {service_msgs__msg__ServiceEventInfo__TYPE_NAME, 33, 33},
    },
    {NULL, 0, 0},
  },
  {
    {s2_msgs__srv__PluginCall_Event__FIELD_NAME__request, 7, 7},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE_BOUNDED_SEQUENCE,
      1,
      0,
      {s2_msgs__srv__PluginCall_Request__TYPE_NAME, 30, 30},
    },
    {NULL, 0, 0},
  },
  {
    {s2_msgs__srv__PluginCall_Event__FIELD_NAME__response, 8, 8},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE_BOUNDED_SEQUENCE,
      1,
      0,
      {s2_msgs__srv__PluginCall_Response__TYPE_NAME, 31, 31},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription s2_msgs__srv__PluginCall_Event__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {builtin_interfaces__msg__Time__TYPE_NAME, 27, 27},
    {NULL, 0, 0},
  },
  {
    {s2_msgs__srv__PluginCall_Request__TYPE_NAME, 30, 30},
    {NULL, 0, 0},
  },
  {
    {s2_msgs__srv__PluginCall_Response__TYPE_NAME, 31, 31},
    {NULL, 0, 0},
  },
  {
    {service_msgs__msg__ServiceEventInfo__TYPE_NAME, 33, 33},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
s2_msgs__srv__PluginCall_Event__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {s2_msgs__srv__PluginCall_Event__TYPE_NAME, 28, 28},
      {s2_msgs__srv__PluginCall_Event__FIELDS, 3, 3},
    },
    {s2_msgs__srv__PluginCall_Event__REFERENCED_TYPE_DESCRIPTIONS, 4, 4},
  };
  if (!constructed) {
    assert(0 == memcmp(&builtin_interfaces__msg__Time__EXPECTED_HASH, builtin_interfaces__msg__Time__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = builtin_interfaces__msg__Time__get_type_description(NULL)->type_description.fields;
    description.referenced_type_descriptions.data[1].fields = s2_msgs__srv__PluginCall_Request__get_type_description(NULL)->type_description.fields;
    description.referenced_type_descriptions.data[2].fields = s2_msgs__srv__PluginCall_Response__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&service_msgs__msg__ServiceEventInfo__EXPECTED_HASH, service_msgs__msg__ServiceEventInfo__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[3].fields = service_msgs__msg__ServiceEventInfo__get_type_description(NULL)->type_description.fields;
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# Request: JSON-\\xd1\\x81\\xd1\\x82\\xd1\\x80\\xd0\\xbe\\xd0\\xba\\xd0\\xb0 \\xd1\\x81 \\xd0\\xbf\\xd0\\xb0\\xd1\\x80\\xd0\\xb0\\xd0\\xbc\\xd0\\xb5\\xd1\\x82\\xd1\\x80\\xd0\\xb0\\xd0\\xbc\\xd0\\xb8 \\xd0\\xb2\\xd1\\x8b\\xd0\\xb7\\xd0\\xbe\\xd0\\xb2\\xd0\\xb0 \\xd0\\xbf\\xd0\\xbb\\xd0\\xb0\\xd0\\xb3\\xd0\\xb8\\xd0\\xbd\\xd0\\xbe\\xd0\\xb2\\xd0\\xbe\\xd0\\xb3\\xd0\\xbe \\xd1\\x81\\xd0\\xb5\\xd1\\x80\\xd0\\xb2\\xd0\\xb8\\xd1\\x81\\xd0\\xb0\n"
  "string request_json\n"
  "---\n"
  "# Response: \\xd0\\xbf\\xd1\\x80\\xd0\\xb8\\xd0\\xb7\\xd0\\xbd\\xd0\\xb0\\xd0\\xba \\xd1\\x83\\xd1\\x81\\xd0\\xbf\\xd0\\xb5\\xd1\\x85\\xd0\\xb0 \\xd0\\xb8 JSON-\\xd1\\x81\\xd1\\x82\\xd1\\x80\\xd0\\xbe\\xd0\\xba\\xd0\\xb0 \\xd1\\x81 \\xd0\\xbe\\xd1\\x82\\xd0\\xb2\\xd0\\xb5\\xd1\\x82\\xd0\\xbe\\xd0\\xbc\n"
  "bool success\n"
  "string response_json";

static char srv_encoding[] = "srv";
static char implicit_encoding[] = "implicit";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
s2_msgs__srv__PluginCall__get_individual_type_description_source(
  const rosidl_service_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {s2_msgs__srv__PluginCall__TYPE_NAME, 22, 22},
    {srv_encoding, 3, 3},
    {toplevel_type_raw_source, 173, 173},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource *
s2_msgs__srv__PluginCall_Request__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {s2_msgs__srv__PluginCall_Request__TYPE_NAME, 30, 30},
    {implicit_encoding, 8, 8},
    {NULL, 0, 0},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource *
s2_msgs__srv__PluginCall_Response__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {s2_msgs__srv__PluginCall_Response__TYPE_NAME, 31, 31},
    {implicit_encoding, 8, 8},
    {NULL, 0, 0},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource *
s2_msgs__srv__PluginCall_Event__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {s2_msgs__srv__PluginCall_Event__TYPE_NAME, 28, 28},
    {implicit_encoding, 8, 8},
    {NULL, 0, 0},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
s2_msgs__srv__PluginCall__get_type_description_sources(
  const rosidl_service_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[6];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 6, 6};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *s2_msgs__srv__PluginCall__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *s2_msgs__srv__PluginCall_Event__get_individual_type_description_source(NULL);
    sources[3] = *s2_msgs__srv__PluginCall_Request__get_individual_type_description_source(NULL);
    sources[4] = *s2_msgs__srv__PluginCall_Response__get_individual_type_description_source(NULL);
    sources[5] = *service_msgs__msg__ServiceEventInfo__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
s2_msgs__srv__PluginCall_Request__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[1];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 1, 1};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *s2_msgs__srv__PluginCall_Request__get_individual_type_description_source(NULL),
    constructed = true;
  }
  return &source_sequence;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
s2_msgs__srv__PluginCall_Response__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[1];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 1, 1};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *s2_msgs__srv__PluginCall_Response__get_individual_type_description_source(NULL),
    constructed = true;
  }
  return &source_sequence;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
s2_msgs__srv__PluginCall_Event__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[5];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 5, 5};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *s2_msgs__srv__PluginCall_Event__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *s2_msgs__srv__PluginCall_Request__get_individual_type_description_source(NULL);
    sources[3] = *s2_msgs__srv__PluginCall_Response__get_individual_type_description_source(NULL);
    sources[4] = *service_msgs__msg__ServiceEventInfo__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
