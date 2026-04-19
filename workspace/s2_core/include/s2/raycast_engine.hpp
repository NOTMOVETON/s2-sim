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

/// Raycast-движок — brute-force по статическим и динамическим примитивам.
class RaycastEngine {
public:
    /// Установить статическую геометрию для raycast.
    void set_static_geometry(const std::vector<WorldPrimitive>& prims);

    /// Установить динамические объекты (агенты) для текущего тика.
    /// Вызывается перед cast() каждый тик, очищается следующим вызовом.
    void set_dynamic_agents(const std::vector<WorldPrimitive>& agent_bounds);

    /// Один луч.
    RaycastResult cast(const Ray& ray) const;

    /// Батч лучей (для лидара).
    std::vector<RaycastResult> cast_batch(const std::vector<Ray>& rays) const;

private:
    std::vector<WorldPrimitive> static_prims_;
    std::vector<WorldPrimitive> dynamic_prims_;  ///< Агенты текущего тика

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

inline void RaycastEngine::set_dynamic_agents(const std::vector<WorldPrimitive>& agent_bounds) {
    dynamic_prims_ = agent_bounds;
}

inline RaycastResult RaycastEngine::cast(const Ray& ray) const {
    RaycastResult best;
    best.distance = std::numeric_limits<double>::infinity();

    // Проверка одного примитива и обновление best при более близком попадании
    auto check_prim = [&](const WorldPrimitive& prim) {
        std::optional<double> t;
        if (prim.type == "box") {
            t = intersect_box(ray, prim);
        } else if (prim.type == "sphere") {
            t = intersect_sphere(ray, prim);
        } else if (prim.type == "cylinder") {
            t = intersect_cylinder(ray, prim);
        }
        if (!t.has_value()) return;
        if (t.value() <= 0.001 || t.value() >= best.distance || t.value() >= ray.max_range) return;

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
            if (len > 0) best.normal = Vec3{d.x()/len, d.y()/len, d.z()/len};
        } else {
            // Для box и cylinder — упрощённая нормаль (v1)
            best.normal = Vec3{-ray.direction.x(), -ray.direction.y(), -ray.direction.z()};
        }
    };

    for (const auto& prim : static_prims_)  check_prim(prim);
    for (const auto& prim : dynamic_prims_) check_prim(prim);

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

/// Построить матрицу R^T (инверсию ZYX-вращения) из позы примитива.
/// R = Rz(yaw)*Ry(pitch)*Rx(roll).  R^T трансформирует мировые координаты в локальные.
inline void build_rotation_transpose(const Pose3D& pose,
                                     double rt[3][3])
{
    const double cy = std::cos(pose.yaw),   sy = std::sin(pose.yaw);
    const double cp = std::cos(pose.pitch), sp = std::sin(pose.pitch);
    const double cr = std::cos(pose.roll),  sr = std::sin(pose.roll);

    // Строки R^T = столбцы R:
    rt[0][0] =  cy*cp;             rt[0][1] =  sy*cp;             rt[0][2] = -sp;
    rt[1][0] =  cy*sp*sr - sy*cr;  rt[1][1] =  sy*sp*sr + cy*cr;  rt[1][2] =  cp*sr;
    rt[2][0] =  cy*sp*cr + sy*sr;  rt[2][1] =  sy*sp*cr - cy*sr;  rt[2][2] =  cp*cr;
}

/// OBB-ray intersection: трансформируем луч в локальное пространство примитива,
/// затем slab-тест по axis-aligned ±half_extent.
inline std::optional<double> RaycastEngine::intersect_box(const Ray& ray, const WorldPrimitive& box) const {
    double rt[3][3];
    build_rotation_transpose(box.pose, rt);

    // Вектор из центра примитива в начало луча
    const double dx = ray.origin.x() - box.pose.x;
    const double dy = ray.origin.y() - box.pose.y;
    const double dz = ray.origin.z() - box.pose.z;

    // Луч в локальных координатах
    const double lox = rt[0][0]*dx + rt[0][1]*dy + rt[0][2]*dz;
    const double loy = rt[1][0]*dx + rt[1][1]*dy + rt[1][2]*dz;
    const double loz = rt[2][0]*dx + rt[2][1]*dy + rt[2][2]*dz;

    const double ldx = rt[0][0]*ray.direction.x() + rt[0][1]*ray.direction.y() + rt[0][2]*ray.direction.z();
    const double ldy = rt[1][0]*ray.direction.x() + rt[1][1]*ray.direction.y() + rt[1][2]*ray.direction.z();
    const double ldz = rt[2][0]*ray.direction.x() + rt[2][1]*ray.direction.y() + rt[2][2]*ray.direction.z();

    const double half_x = box.size.x() / 2.0;
    const double half_y = box.size.y() / 2.0;
    const double half_z = box.size.z() / 2.0;

    // Slab-тест в локальных координатах
    double tmin = -std::numeric_limits<double>::infinity();
    double tmax =  std::numeric_limits<double>::infinity();

    auto slab = [&](double lo, double ld, double half) -> bool {
        if (std::abs(ld) < 1e-8) {
            // Луч параллелен плоскостям оси — попадание только если начало внутри
            return std::abs(lo) <= half;
        }
        double inv = 1.0 / ld;
        double t1 = (-half - lo) * inv;
        double t2 = ( half - lo) * inv;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        return tmin <= tmax;
    };

    if (!slab(lox, ldx, half_x)) return std::nullopt;
    if (!slab(loy, ldy, half_y)) return std::nullopt;
    if (!slab(loz, ldz, half_z)) return std::nullopt;

    if (tmin < 0.001) tmin = tmax;  // начало луча внутри box
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

/// OBB-ray intersection для цилиндра: трансформируем луч в локальное пространство,
/// тест боковой поверхности вдоль локальной оси Z + проверка высоты.
inline std::optional<double> RaycastEngine::intersect_cylinder(const Ray& ray, const WorldPrimitive& cyl) const {
    double rt[3][3];
    build_rotation_transpose(cyl.pose, rt);

    // Луч в локальных координатах цилиндра
    const double dx = ray.origin.x() - cyl.pose.x;
    const double dy = ray.origin.y() - cyl.pose.y;
    const double dz = ray.origin.z() - cyl.pose.z;

    const double lox = rt[0][0]*dx + rt[0][1]*dy + rt[0][2]*dz;
    const double loy = rt[1][0]*dx + rt[1][1]*dy + rt[1][2]*dz;
    const double loz = rt[2][0]*dx + rt[2][1]*dy + rt[2][2]*dz;

    const double ldx = rt[0][0]*ray.direction.x() + rt[0][1]*ray.direction.y() + rt[0][2]*ray.direction.z();
    const double ldy = rt[1][0]*ray.direction.x() + rt[1][1]*ray.direction.y() + rt[1][2]*ray.direction.z();
    const double ldz = rt[2][0]*ray.direction.x() + rt[2][1]*ray.direction.y() + rt[2][2]*ray.direction.z();

    // Боковая поверхность: тест по XY (ось цилиндра — Z в локальных координатах)
    const double a = ldx*ldx + ldy*ldy;
    const double b = lox*ldx + loy*ldy;
    const double c = lox*lox + loy*loy - cyl.radius*cyl.radius;

    if (std::abs(a) < 1e-12) return std::nullopt;  // луч параллелен оси

    const double disc = b*b - a*c;
    if (disc < 0) return std::nullopt;

    const double sqrt_disc = std::sqrt(disc);
    double t = (-b - sqrt_disc) / a;

    if (t < 0.001) {
        t = (-b + sqrt_disc) / a;
        if (t < 0.001) return std::nullopt;
    }

    // Проверка: точка попадания внутри высоты цилиндра (в локальных координатах)
    const double local_hit_z = loz + ldz * t;
    if (std::abs(local_hit_z) > cyl.height / 2.0) {
        return std::nullopt;
    }

    return t;
}

} // namespace s2