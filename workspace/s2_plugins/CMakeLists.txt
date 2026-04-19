# s2_plugins — плагины агента (регистрация)

add_library(s2_plugins STATIC
    src/plugins_registry.cpp
    src/color.cpp
    src/joint_vel.cpp
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