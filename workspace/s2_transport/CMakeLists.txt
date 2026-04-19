cmake_minimum_required(VERSION 3.22)
project(s2_transport LANGUAGES CXX)

option(S2_WITH_ROS2 "Build with ROS2 transport" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(S2_WITH_ROS2)
    find_package(rclcpp REQUIRED)
    find_package(geometry_msgs REQUIRED)
    find_package(sensor_msgs REQUIRED)
    find_package(nav_msgs REQUIRED)
    find_package(std_srvs REQUIRED)
    find_package(std_msgs REQUIRED)
    find_package(tf2_ros REQUIRED)

    # s2_msgs — опциональный кастомный пакет (собирается отдельно через colcon)
    find_package(s2_msgs QUIET)
    if(s2_msgs_FOUND)
        message(STATUS "[s2_transport] s2_msgs found — PluginCall services enabled")
    else()
        message(STATUS "[s2_transport] s2_msgs NOT found — services fall back to std_srvs/Trigger")
    endif()

    add_library(s2_transport STATIC
        src/ros2_transport.cpp              # MVP: /cmd_vel (сохранён для совместимости)
        src/ros2_transport_adapter.cpp      # Новый полный адаптер
        src/sim_transport_bridge.cpp        # Bridge (без ROS2 типов)
    )

    target_include_directories(s2_transport PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    ament_target_dependencies(s2_transport
        rclcpp
        geometry_msgs
        sensor_msgs
        nav_msgs
        std_srvs
        std_msgs
        tf2_ros
    )

    if(s2_msgs_FOUND)
        ament_target_dependencies(s2_transport s2_msgs)
        target_compile_definitions(s2_transport PUBLIC S2_WITH_S2_MSGS)
    endif()

    target_link_libraries(s2_transport
        s2_core
    )

    target_compile_definitions(s2_transport PUBLIC S2_WITH_ROS2)

else()
    # Stub-сборка без ROS2
    add_library(s2_transport STATIC
        src/ros2_transport_stub.cpp           # MVP stub (сохранён)
        src/ros2_transport_adapter_stub.cpp   # Новый адаптер stub
        src/sim_transport_bridge.cpp          # Bridge компилируется всегда
    )

    target_include_directories(s2_transport PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(s2_transport PUBLIC
        s2_core
    )
endif()
