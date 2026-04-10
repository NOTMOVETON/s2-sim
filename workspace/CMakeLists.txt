cmake_minimum_required(VERSION 3.16)
project(s2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Зависимости
find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json 3.0 REQUIRED)
find_package(GTest REQUIRED)
find_package(OpenSSL REQUIRED)

# GeographicLib: Find-скрипт лежит в /usr/share/cmake/geographiclib
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/usr/share/cmake/geographiclib")
find_package(GeographicLib REQUIRED)

# FindGeographicLib.cmake не создаёт imported target, создаём вручную
# В Ubuntu Noble библиотека называется libGeographicLib.so, а не libGeographic.so
if(NOT TARGET GeographicLib::GeographicLib)
    add_library(GeographicLib::GeographicLib INTERFACE IMPORTED)
    set_target_properties(GeographicLib::GeographicLib PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${GeographicLib_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "GeographicLib"
    )
endif()

enable_testing()

add_subdirectory(s2_core)
add_subdirectory(s2_plugins)
add_subdirectory(s2_visualizer)
add_subdirectory(s2_transport)
