#pragma once

/**
 * @file sensor_data.hpp
 * Типы данных сенсоров для хранения в SharedState агента.
 */

#include <string>
#include <vector>

namespace s2
{

/**
 * @brief Данные GNSS сенсора.
 */
struct GnssData
{
    uint64_t seq{0};        ///< Монотонно растущий счётчик; меняется только при новой публикации
    double lat{0.0};
    double lon{0.0};
    double alt{0.0};
    double azimuth{0.0};
    double accuracy{0.0};
};

/**
 * @brief Данные IMU сенсора.
 */
struct ImuData
{
    uint64_t seq{0};        ///< Монотонно растущий счётчик
    double gyro_x{0.0};
    double gyro_y{0.0};
    double gyro_z{0.0};
    double accel_x{0.0};
    double accel_y{0.0};
    double accel_z{9.81};
    double yaw{0.0};
};

/**
 * @brief Данные DiffDrive привода.
 */
struct DiffDriveData
{
    uint64_t seq{0};        ///< Монотонно растущий счётчик
    double desired_linear{0.0};
    double desired_angular{0.0};
    double max_linear{2.0};
    double max_angular{1.5};
    std::vector<std::string> warnings;
};

} // namespace s2