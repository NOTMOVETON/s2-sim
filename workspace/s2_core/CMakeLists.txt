find_package(tinyxml2 REQUIRED)

add_library(s2_core STATIC
    src/placeholder.cpp
    src/world_snapshot.cpp
    src/sim_engine_viz.cpp
    src/kinematic_tree.cpp
    src/urdf_loader.cpp
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
