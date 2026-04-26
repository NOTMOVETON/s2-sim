# s2_plugins — плагины агента (регистрация)

add_library(s2_plugins STATIC
    src/plugins_registry.cpp
    src/behaviors_registry.cpp
    src/behaviors/door_behavior.cpp
    src/plugins/door_opener_plugin.cpp
    src/plugins/door_wire_controller.cpp
    src/plugins/conveyor_wire_controller.cpp
    src/plugins/event_reactor.cpp
    src/color.cpp
    src/joint_vel.cpp
    src/effects_registry.cpp
)

target_include_directories(s2_plugins PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(s2_plugins PUBLIC
    s2_core
)

target_link_libraries(s2_plugins PRIVATE
    GeographicLib::GeographicLib
)
# Тесты плагинов эффектов
add_executable(s2_plugins_tests
    tests/test_effect_fog_emi.cpp
)
target_link_libraries(s2_plugins_tests PRIVATE s2_plugins s2_core GTest::gtest_main)
add_test(NAME s2_plugins_tests COMMAND s2_plugins_tests)

# Тесты DoorBehavior
add_executable(s2_door_behavior_tests
    tests/test_door_behavior.cpp
)
target_link_libraries(s2_door_behavior_tests PRIVATE s2_plugins s2_core GTest::gtest_main)
add_test(NAME s2_door_behavior_tests COMMAND s2_door_behavior_tests)
