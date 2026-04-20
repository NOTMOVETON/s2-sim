find_package(tinyxml2 QUIET)
if(NOT tinyxml2_FOUND)
    # Ubuntu 22.04 — libtinyxml2-dev не предоставляет CMake-конфиг, ищем вручную
    find_library(TINYXML2_LIB NAMES tinyxml2 REQUIRED)
    find_path(TINYXML2_INCLUDE_DIR NAMES tinyxml2.h PATH_SUFFIXES tinyxml2)
    if(NOT TINYXML2_LIB)
        message(FATAL_ERROR "tinyxml2 library not found")
    endif()
    add_library(tinyxml2::tinyxml2 INTERFACE IMPORTED)
    set_target_properties(tinyxml2::tinyxml2 PROPERTIES
        INTERFACE_LINK_LIBRARIES "${TINYXML2_LIB}"
    )
    if(TINYXML2_INCLUDE_DIR)
        set_property(TARGET tinyxml2::tinyxml2 PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES "${TINYXML2_INCLUDE_DIR}")
    endif()
endif()

add_library(s2_core STATIC
    src/placeholder.cpp
    src/world_snapshot.cpp
    src/sim_engine_viz.cpp
    src/kinematic_tree.cpp
    src/urdf_loader.cpp
    src/zone_system.cpp
)

target_include_directories(s2_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(s2_core PUBLIC
    Eigen3::Eigen
    yaml-cpp
    nlohmann_json::nlohmann_json
    GeographicLib::GeographicLib
    tinyxml2::tinyxml2
)

# Тесты
add_executable(s2_core_tests
    tests/test_smoke.cpp
    tests/test_types.cpp
    tests/test_shared_state.cpp
    tests/test_sim_bus.cpp
    tests/test_sim_engine.cpp
    tests/test_geometry.cpp
    tests/test_world_snapshot.cpp
    tests/test_snapshot_viz.cpp
    tests/test_plugin_input.cpp
    tests/test_triple_buffer.cpp
    tests/test_ros2_transport.cpp
    tests/test_sim_transport_bridge.cpp
    tests/test_kinematic_tree.cpp
    tests/test_urdf_loader.cpp
    tests/test_joint_vel_plugin.cpp
    tests/test_color_plugin.cpp
    tests/test_collision_system.cpp
    tests/test_gravity_plugin.cpp
    tests/test_lidar_plugin.cpp
    tests/test_zone_system.cpp
    tests/test_effect_modifier.cpp
    tests/test_effect_velocity_addition.cpp
    tests/test_effect_charging.cpp
    tests/test_battery_plugin.cpp
)
target_link_libraries(s2_core_tests PRIVATE s2_core s2_plugins s2_transport GTest::gtest_main)

# Копируем тестовые данные рядом с бинарником и передаём путь компилятору
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_urdf.xml
    ${CMAKE_CURRENT_BINARY_DIR}/test_urdf.xml
    COPYONLY
)
target_compile_definitions(s2_core_tests PRIVATE
    S2_TEST_URDF_PATH="${CMAKE_CURRENT_BINARY_DIR}/test_urdf.xml"
)
# Ensure ROS2 symbols are available when building the transport tests
target_compile_definitions(s2_core_tests PRIVATE S2_WITH_ROS2)
add_test(NAME s2_core_tests COMMAND s2_core_tests)

# ── Тесты редактора сцены (без ROS2, только s2_core + s2_plugins) ──
add_executable(s2_editor_tests
    tests/test_scene_writer.cpp
)
target_link_libraries(s2_editor_tests PRIVATE s2_core s2_plugins GTest::gtest_main)
add_test(NAME s2_editor_tests COMMAND s2_editor_tests)
