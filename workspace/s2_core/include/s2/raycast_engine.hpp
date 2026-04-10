#pragma once

/**
 * @file raycast_engine.hpp
 * RaycastEngine — brute-force raycast по статической геометрии мира.
 *
 * v1: только статические примитивы (box, sphere, cylinder).
 * Без BVH — достаточно для 10-20 примитивов.
 * Без динамических объектов (добавим позже).
 */

#include <s2/types.hpp>
#include <s2/world.hpp>

#include <vector>
#include <optional>
#include <limits>
#include <cmath>
#include <algorithm>

namespace s2 {

/// Луч: начало + нормализованное направление.
struct Ray {
    Vec3 origin;         ///< Точка начала луча
    Vec3 direction;      ///< Нормализованное направление
    double max_range{30.0};
};

/// Результат пересечения луча.
struct RaycastResult {
    bool hit{false};
    double distance{0.0};
    Vec3 point{0, 0, 0};
    Vec3 normal{0, 0, 1};
};

/// Raycast-движок — brute-force по статическим примитивам.
class RaycastEngine {
public:
    /// Установить геометрию для raycast.
    void set_static_geometry(const std::vector<WorldPrimitive>& prims);

    /// Один луч.
    RaycastResult cast(const Ray& ray) const;

    /// Батч лучей (для лидара).
    std::vector<RaycastResult> cast_batch(const std::vector<Ray>& rays) const;

private:
    std::vector<WorldPrimitive> static_prims_;

    /// Ray-box intersection (AABB).
    std::optional<double> intersect_box(const Ray& ray, const WorldPrimitive& box) const;

    /// Ray-sphere intersection.
    std::optional<double> intersect_sphere(const Ray& ray, const WorldPrimitive& sphere) const;

    /// Ray-cylinder intersection (вертикальный, по Z).
    std::optional<double> intersect_cylinder(const Ray& ray, const WorldPrimitive& cyl) const;
};

// ─── Implementation ────────────────────────────────────────────────────

inline void RaycastEngine::set_static_geometry(const std::vector<WorldPrimitive>& prims) {
    static_prims_ = prims;
}

inline RaycastResult RaycastEngine::cast(const Ray& ray) const {
    RaycastResult best;
    best.distance = std::numeric_limits<double>::infinity();

    for (const auto& prim : static_prims_) {
        std::optional<double> t;

        if (prim.type == "box") {
            t = intersect_box(ray, prim);
        } else if (prim.type == "sphere") {
            t = intersect_sphere(ray, prim);
        } else if (prim.type == "cylinder") {
            t = intersect_cylinder(ray, prim);
        }

        if (t.has_value() && t.value() > 0.001 && t.value() < best.distance && t.value() < ray.max_range) {
            best.distance = t.value();
            best.hit = true;
            best.point = Vec3{
                ray.origin.x() + ray.direction.x() * best.distance,
                ray.origin.y() + ray.direction.y() * best.distance,
                ray.origin.z() + ray.direction.z() * best.distance
            };
            // Нормаль: от центра примитива к точке попадания
            if (prim.type == "sphere") {
                Vec3 d{
                    best.point.x() - prim.pose.x,
                    best.point.y() - prim.pose.y,
                    best.point.z() - prim.pose.z
                };
                double len = std::sqrt(d.x()*d.x() + d.y()*d.y() + d.z()*d.z());
                if (len > 0) {
                    best.normal = Vec3{d.x()/len, d.y()/len, d.z()/len};
                }
            }
            // Для box и cylinder — упрощённый нормаль (для v1)
            else {
                best.normal = Vec3{
                    -ray.direction.x(),
                    -ray.direction.y(),
                    -ray.direction.z()
                };
            }
        }
    }

    return best;
}

inline std::vector<RaycastResult> RaycastEngine::cast_batch(const std::vector<Ray>& rays) const {
    std::vector<RaycastResult> results;
    results.reserve(rays.size());
    for (const auto& ray : rays) {
        results.push_back(cast(ray));
    }
    return results;
}

/// Ray-AABB intersection (slab method).
inline std::optional<double> RaycastEngine::intersect_box(const Ray& ray, const WorldPrimitive& box) const {
    double half_x = box.size.x() / 2.0;
    double half_y = box.size.y() / 2.0;
    double half_z = box.size.z() / 2.0;

    // AABB min/max
    double min_x = box.pose.x - half_x;
    double max_x = box.pose.x + half_x;
    double min_y = box.pose.y - half_y;
    double max_y = box.pose.y + half_y;
    double min_z = box.pose.z - half_z;
    double max_z = box.pose.z + half_z;

    // Slab intersection
    double tmin = -std::numeric_limits<double>::infinity();
    double tmax = std::numeric_limits<double>::infinity();

    double inv_dx = (std::abs(ray.direction.x()) < 1e-8) ? 1e8 : 1.0 / ray.direction.x();
    double t1 = (min_x - ray.origin.x()) * inv_dx;
    double t2 = (max_x - ray.origin.x()) * inv_dx;
    double s1 = std::min(t1, t2);
    double s2 = std::max(t1, t2);
    tmin = std::max(tmin, s1);
    tmax = std::min(tmax, s2);
    if (tmin > tmax) return std::nullopt;

    double inv_dy = (std::abs(ray.direction.y()) < 1e-8) ? 1e8 : 1.0 / ray.direction.y();
    t1 = (min_y - ray.origin.y()) * inv_dy;
    t2 = (max_y - ray.origin.y()) * inv_dy;
    s1 = std::min(t1, t2);
    s2 = std::max(t1, t2);
    tmin = std::max(tmin, s1);
    tmax = std::min(tmax, s2);
    if (tmin > tmax) return std::nullopt;

    double inv_dz = (std::abs(ray.direction.z()) < 1e-8) ? 1e8 : 1.0 / ray.direction.z();
    t1 = (min_z - ray.origin.z()) * inv_dz;
    t2 = (max_z - ray.origin.z()) * inv_dz;
    s1 = std::min(t1, t2);
    s2 = std::max(t1, t2);
    tmin = std::max(tmin, s1);
    tmax = std::min(tmax, s2);
    if (tmin > tmax) return std::nullopt;

    if (tmin < 0.001) tmin = tmax;  // camera inside box
    if (tmin < 0.001) return std::nullopt;

    return tmin;
}

/// Ray-sphere intersection.
inline std::optional<double> RaycastEngine::intersect_sphere(const Ray& ray, const WorldPrimitive& sphere) const {
    double dx = ray.origin.x() - sphere.pose.x;
    double dy = ray.origin.y() - sphere.pose.y;
    double dz = ray.origin.z() - sphere.pose.z;

    double b = dx * ray.direction.x() + dy * ray.direction.y() + dz * ray.direction.z();
    double c = dx*dx + dy*dy + dz*dz - sphere.radius * sphere.radius;

    double disc = b*b - c;
    if (disc < 0) return std::nullopt;

    double sqrt_disc = std::sqrt(disc);
    double t = -b - sqrt_disc;
    if (t < 0.001) {
        t = -b + sqrt_disc;
        if (t < 0.001) return std::nullopt;
    }
    return t;
}

/// Ray-cylinder intersection (vertical along Z).
inline std::optional<double> RaycastEngine::intersect_cylinder(const Ray& ray, const WorldPrimitive& cyl) const {
    // Проверяем только боковую поверхность (без крышки для простоты)
    double dx = ray.origin.x() - cyl.pose.x;
    double dy = ray.origin.y() - cyl.pose.y;
    double dir_x = ray.direction.x();
    double dir_y = ray.direction.y();

    double a = dir_x*dir_x + dir_y*dir_y;
    double b = dx*dir_x + dy*dir_y;
    double c = dx*dx + dy*dy - cyl.radius*cyl.radius;

    double disc = b*b - a*c;
    if (disc < 0) return std::nullopt;

    double sqrt_disc = std::sqrt(disc);
    double t = (-b - sqrt_disc) / a;

    if (t < 0.001) {
        t = (-b + sqrt_disc) / a;
        if (t < 0.001) return std::nullopt;
    }

    // Проверка: точка пересечения внутри высоты цилиндра
    double hit_z = ray.origin.z() + ray.direction.z() * t;
    if (hit_z < cyl.pose.z - cyl.height/2.0 || hit_z > cyl.pose.z + cyl.height/2.0) {
        return std::nullopt;
    }

    return t;
}

} // namespace s2