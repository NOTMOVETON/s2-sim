# s2_plugins — плагины агента (регистрация)

add_library(s2_plugins STATIC
    src/plugins_registry.cpp
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
