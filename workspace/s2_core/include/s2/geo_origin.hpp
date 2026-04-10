#pragma once

/**
 * @file geo_origin.hpp
 * Глобальная LLA точка отсчёта для сцены.
 * Определяет, где находится начало локальных координат (0, 0, 0).
 */

namespace s2
{

/**
 * @brief Начальная точка LLA для всей сцены.
 * Определяет где находится (0, 0, 0) локальных координат.
 * Заполняется из YAML: s2.world.geo_origin: {lat: ..., lon: ..., alt: ...}
 */
struct GeoOrigin
{
    double lat{0.0};  ///< Широта начала (градусы)
    double lon{0.0};  ///< Долгота начала (градусы)
    double alt{0.0};  ///< Высота начала (метры над эллипсоидом WGS84)

    bool is_set() const { return lat != 0.0 || lon != 0.0 || alt != 0.0; }
};

} // namespace s2