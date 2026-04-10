add_executable(s2_sim
    src/viz_server.cpp
    src/main.cpp
    src/sim_engine_viz_impl.cpp
)

target_include_directories(s2_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../s2_core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../s2_plugins/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../s2_transport/include
)

# Линкуем s2_transport (stub или ROS2 версия в зависимости от S2_WITH_ROS2)
target_link_libraries(s2_sim PRIVATE
    s2_core
    s2_plugins
    s2_transport
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)