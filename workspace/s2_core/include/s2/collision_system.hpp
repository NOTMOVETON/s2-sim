#pragma once

/**
 * @file collision_system.hpp
 * CollisionSystem — система коллизий агентов со статической геометрией сцены.
 *
 * Работает в фазе 3h тикового цикла SimEngine.
 * Поддерживает только сферический bounding volume агента (v1).
 *
 * Ключевые принципы:
 *  - Нет специального типа "пол". Поведение определяется по нормали грани.
 *  - Нормаль вверх (box сверху) → пол/пандус.
 *  - Нормаль горизонтально (боковина) → стена.
 *  - max_slope_rad определяет угол, до которого поверхность считается проходимой.
 *  - max_step_height позволяет переезжать мелкие ступеньки и стыки поверхностей.
 *
 * Интерфейс с GravityPlugin (задача 21): find_support_surface().
 * Интерфейс с LidarPlugin (задача 22): CollisionShape агента хранится в Agent.
 */

#include <s2/types.hpp>
#include <s2/world.hpp>

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace s2
{

/**
 * @brief Результат проверки коллизии сферы с одним примитивом.
 */
struct CollisionContact
{
    bool   has_contact{false};
    Vec3   contact_normal{0, 0, 1};  ///< Нормаль (от поверхности к агенту), нормализована
    double penetration{0.0};         ///< Глубина проникновения (метры)
    double obstacle_top_z{0.0};      ///< Z верхней точки примитива (для проверки max_step_height)
};

/**
 * @brief Результат raycast: Z точки пересечения + нормаль поверхности.
 */
struct RayHit
{
    double z;
    Vec3   normal;
};

/**
 * @brief Информация об опорной поверхности: Z поверхности + нормаль.
 * ground_z — это Z самой поверхности (не центра агента).
 */
struct SupportInfo
{
    double ground_z;
    Vec3   normal;
};

/**
 * @brief Система коллизий агентов со статической геометрией сцены.
 *
 * Используется SimEngine в фазе 3h.
 * Геометрия передаётся один раз при загрузке сцены через set_static_geometry().
 */
class CollisionSystem
{
public:
    /**
     * @brief Установить статическую геометрию сцены.
     * Вызывается при загрузке/перезагрузке сцены.
     */
    void set_static_geometry(const std::vector<WorldPrimitive>& prims);

    /**
     * @brief Проверить коллизию сферы со всей статической геометрией.
     *
     * Возвращает все контакты, отсортированные по убыванию penetration
     * (самый глубокий первый). Пустой вектор = нет коллизий.
     *
     * @param center Центр сферы в мировых координатах
     * @param radius Радиус сферы
     */
    std::vector<CollisionContact> check_sphere_all(
        const Vec3& center, double radius) const;

    /**
     * @brief Slide-реакция: убрать компоненту velocity, направленную в поверхность.
     *
     * contact_normal должна быть нормализована.
     * Если velocity уже направлена ОТ поверхности — не меняем ничего.
     *
     * @param vel     Текущая скорость агента
     * @param normal  Нормаль контакта (от поверхности к агенту)
     * @return        Скорректированная скорость
     */
    static Velocity apply_slide(const Velocity& vel, const Vec3& normal);

    /**
     * @brief Найти опорную поверхность под агентом (для GravityPlugin, задача 21).
     *
     * Бросает луч вниз (-Z) из нижней точки сферы агента.
     * Возвращает Z поверхности, если она есть в диапазоне [0, 2.0] метра ниже.
     *
     * @param position       Центр сферы агента в мировых координатах
     * @param bounding_radius Радиус сферы
     * @return               Z поверхности или nullopt если опоры нет
     */
    std::optional<SupportInfo> find_support_surface(
        const Vec3& position, double bounding_radius) const;

    // Построить полную матрицу вращения ZYX из Pose3D.
    // Public: используется SimEngine для преобразования body -> world.
    static Eigen::Matrix3d rotation_from_pose(const Pose3D& pose);

private:
    std::vector<WorldPrimitive> static_prims_;

    // ─── Проверка sphere vs примитив ────────────────────────────────────────

    CollisionContact check_sphere_vs_box(
        const Vec3& center, double radius, const WorldPrimitive& box) const;

    CollisionContact check_sphere_vs_sphere(
        const Vec3& center, double radius, const WorldPrimitive& sph) const;

    CollisionContact check_sphere_vs_cylinder(
        const Vec3& center, double radius, const WorldPrimitive& cyl) const;

    // ─── Raycast вниз для find_support_surface ───────────────────────────────

    std::optional<RayHit> ray_down_vs_box(
        const Vec3& origin, const WorldPrimitive& box) const;

    std::optional<RayHit> ray_down_vs_cylinder(
        const Vec3& origin, const WorldPrimitive& cyl) const;

    std::optional<RayHit> ray_down_vs_sphere(
        const Vec3& origin, const WorldPrimitive& sph) const;

    // ─── Вспомогательные функции ────────────────────────────────────────────

    // Верхняя точка примитива по Z в мировых координатах (для obstacle_top_z).
    static double primitive_top_z(const WorldPrimitive& prim);
};

// ============================================================================
// Inline-реализация
// ============================================================================

inline void CollisionSystem::set_static_geometry(
    const std::vector<WorldPrimitive>& prims)
{
    static_prims_ = prims;
}

// ─── Вспомогательные функции ────────────────────────────────────────────────

inline Eigen::Matrix3d CollisionSystem::rotation_from_pose(const Pose3D& pose)
{
    // ZYX Эйлер: Rz(yaw) * Ry(pitch) * Rx(roll)
    Eigen::AngleAxisd rx(pose.roll,  Vec3::UnitX());
    Eigen::AngleAxisd ry(pose.pitch, Vec3::UnitY());
    Eigen::AngleAxisd rz(pose.yaw,   Vec3::UnitZ());
    return (rz * ry * rx).toRotationMatrix();
}

inline double CollisionSystem::primitive_top_z(const WorldPrimitive& prim)
{
    if (prim.type == "box") {
        // Верхняя точка повёрнутого box — приближение через half-extents
        // Для горизонтальных box (pitch≈0) это точное значение.
        // Для наклонных — консервативная оценка (завышенная).
        Eigen::Matrix3d R = rotation_from_pose(prim.pose);
        Vec3 half{prim.size.x() / 2.0, prim.size.y() / 2.0, prim.size.z() / 2.0};
        // Максимальное Z-смещение угловых точек = сумма |R_col * half_i|
        double max_z_offset =
            std::abs(R(2, 0)) * half.x() +
            std::abs(R(2, 1)) * half.y() +
            std::abs(R(2, 2)) * half.z();
        return prim.pose.z + max_z_offset;
    }
    if (prim.type == "cylinder") {
        return prim.pose.z + prim.height / 2.0;
    }
    if (prim.type == "sphere") {
        return prim.pose.z + prim.radius;
    }
    return prim.pose.z;
}

// ─── check_sphere_vs_box ────────────────────────────────────────────────────

inline CollisionContact CollisionSystem::check_sphere_vs_box(
    const Vec3& center, double radius, const WorldPrimitive& box) const
{
    // Полная ZYX-ротация: трансформируем центр сферы в систему box
    Eigen::Matrix3d R = rotation_from_pose(box.pose);
    Vec3 box_pos{box.pose.x, box.pose.y, box.pose.z};

    // Позиция центра сферы в локальной системе box
    Vec3 local = R.transpose() * (center - box_pos);

    Vec3 half{box.size.x() / 2.0, box.size.y() / 2.0, box.size.z() / 2.0};

    // Ближайшая точка AABB к локальному центру
    Vec3 nearest{
        std::clamp(local.x(), -half.x(), half.x()),
        std::clamp(local.y(), -half.y(), half.y()),
        std::clamp(local.z(), -half.z(), half.z())
    };

    Vec3 e = local - nearest;
    double dist2 = e.squaredNorm();

    if (dist2 >= radius * radius) return {};  // нет контакта

    CollisionContact c;
    c.has_contact = true;

    // obstacle_top_z: Z верхней грани box в проекции центра сферы.
    // Для горизонтальных box (стены, полы) это совпадает с primitive_top_z().
    // Для наклонных (рампы) — локальная высота, а не глобальный максимум,
    // что позволяет агенту корректно заезжать на рампу снизу.
    Vec3 top_face_local{
        std::clamp(local.x(), -half.x(), half.x()),
        std::clamp(local.y(), -half.y(), half.y()),
        half.z()
    };
    Vec3 top_face_world = R * top_face_local + box_pos;
    c.obstacle_top_z = top_face_world.z();

    double dist = std::sqrt(dist2);
    c.penetration = radius - dist;

    // Нормаль: от поверхности к центру сферы
    Vec3 local_normal;
    if (dist > 1e-9) {
        local_normal = e / dist;
    } else {
        // Центр сферы внутри box — выдавить по оси минимального перекрытия
        double ox = half.x() - std::abs(local.x());
        double oy = half.y() - std::abs(local.y());
        double oz = half.z() - std::abs(local.z());
        if (ox <= oy && ox <= oz) {
            local_normal = Vec3{local.x() >= 0.0 ? 1.0 : -1.0, 0.0, 0.0};
            c.penetration = radius + ox;
        } else if (oy <= ox && oy <= oz) {
            local_normal = Vec3{0.0, local.y() >= 0.0 ? 1.0 : -1.0, 0.0};
            c.penetration = radius + oy;
        } else {
            local_normal = Vec3{0.0, 0.0, local.z() >= 0.0 ? 1.0 : -1.0};
            c.penetration = radius + oz;
        }
    }

    // Преобразуем нормаль обратно в мировые координаты
    c.contact_normal = (R * local_normal).normalized();
    return c;
}

// ─── check_sphere_vs_sphere ─────────────────────────────────────────────────

inline CollisionContact CollisionSystem::check_sphere_vs_sphere(
    const Vec3& center, double radius, const WorldPrimitive& sph) const
{
    Vec3 sph_pos{sph.pose.x, sph.pose.y, sph.pose.z};
    Vec3 delta = center - sph_pos;
    double dist = delta.norm();
    double sum_r = radius + sph.radius;

    if (dist >= sum_r) return {};

    CollisionContact c;
    c.has_contact = true;
    c.penetration = sum_r - dist;
    c.obstacle_top_z = primitive_top_z(sph);

    if (dist > 1e-9) {
        c.contact_normal = delta / dist;
    } else {
        c.contact_normal = Vec3{0, 0, 1};
    }
    return c;
}

// ─── check_sphere_vs_cylinder ───────────────────────────────────────────────

inline CollisionContact CollisionSystem::check_sphere_vs_cylinder(
    const Vec3& center, double radius, const WorldPrimitive& cyl) const
{
    // Цилиндр ориентирован вдоль локальной оси Z
    Eigen::Matrix3d R = rotation_from_pose(cyl.pose);
    Vec3 cyl_pos{cyl.pose.x, cyl.pose.y, cyl.pose.z};
    Vec3 local = R.transpose() * (center - cyl_pos);

    double half_h = cyl.height / 2.0;

    // Радиальное расстояние в XY-плоскости цилиндра
    double radial = std::sqrt(local.x() * local.x() + local.y() * local.y());
    double lz     = local.z();

    // Ближайшая точка на цилиндре
    double near_r  = std::min(radial, cyl.radius);
    double near_z  = std::clamp(lz, -half_h, half_h);
    double near_x, near_y;

    if (radial > 1e-9) {
        near_x = near_r * local.x() / radial;
        near_y = near_r * local.y() / radial;
    } else {
        near_x = 0.0;
        near_y = 0.0;
    }

    Vec3 e{local.x() - near_x, local.y() - near_y, lz - near_z};
    double dist2 = e.squaredNorm();

    if (dist2 >= radius * radius) return {};

    CollisionContact c;
    c.has_contact = true;
    c.obstacle_top_z = primitive_top_z(cyl);

    double dist = std::sqrt(dist2);
    c.penetration = radius - dist;

    Vec3 local_normal;
    if (dist > 1e-9) {
        local_normal = e / dist;
    } else {
        // Центр сферы на поверхности цилиндра — выдавить радиально или по Z
        bool on_cap = (std::abs(lz) >= half_h - 1e-9);
        if (on_cap) {
            local_normal = Vec3{0.0, 0.0, lz >= 0.0 ? 1.0 : -1.0};
            c.penetration = radius + (half_h - std::abs(lz));
        } else {
            local_normal = Vec3{local.x() / (radial + 1e-12),
                                local.y() / (radial + 1e-12), 0.0};
            c.penetration = radius + (cyl.radius - radial);
        }
    }

    c.contact_normal = (R * local_normal).normalized();
    return c;
}

// ─── check_sphere_all ───────────────────────────────────────────────────────

inline std::vector<CollisionContact> CollisionSystem::check_sphere_all(
    const Vec3& center, double radius) const
{
    std::vector<CollisionContact> contacts;
    contacts.reserve(4);

    for (const auto& prim : static_prims_) {
        CollisionContact c;
        if (prim.type == "box") {
            c = check_sphere_vs_box(center, radius, prim);
        } else if (prim.type == "sphere") {
            c = check_sphere_vs_sphere(center, radius, prim);
        } else if (prim.type == "cylinder") {
            c = check_sphere_vs_cylinder(center, radius, prim);
        }
        if (c.has_contact) {
            contacts.push_back(c);
        }
    }

    // Сортировка по убыванию penetration: сначала самый глубокий контакт
    std::sort(contacts.begin(), contacts.end(),
        [](const CollisionContact& a, const CollisionContact& b) {
            return a.penetration > b.penetration;
        });

    return contacts;
}

// ─── apply_slide ────────────────────────────────────────────────────────────

inline Velocity CollisionSystem::apply_slide(
    const Velocity& vel, const Vec3& normal)
{
    Velocity result = vel;
    double proj = vel.linear.dot(normal);
    // Убираем только компоненту, направленную В поверхность (proj < 0)
    if (proj < 0.0) {
        result.linear -= normal * proj;
    }
    return result;
}

// ─── find_support_surface ───────────────────────────────────────────────────

inline std::optional<SupportInfo> CollisionSystem::find_support_surface(
    const Vec3& position, double bounding_radius) const
{
    // Луч из нижней точки сферы, с небольшим смещением вверх, чтобы не
    // столкнуться с поверхностью на которой агент уже стоит
    Vec3 origin{position.x(), position.y(),
                position.z() - bounding_radius + 0.01};

    constexpr double max_dist = 2.0;  // смотрим не дальше 2 метра вниз
    std::optional<RayHit> best;

    for (const auto& prim : static_prims_) {
        std::optional<RayHit> hit;
        if (prim.type == "box") {
            hit = ray_down_vs_box(origin, prim);
        } else if (prim.type == "cylinder") {
            hit = ray_down_vs_cylinder(origin, prim);
        } else if (prim.type == "sphere") {
            hit = ray_down_vs_sphere(origin, prim);
        }

        if (hit && (origin.z() - hit->z) <= max_dist) {
            if (!best || hit->z > best->z) {
                best = hit;
            }
        }
    }

    if (!best) return std::nullopt;
    return SupportInfo{best->z, best->normal};
}

// ─── ray_down_vs_box ────────────────────────────────────────────────────────

inline std::optional<RayHit> CollisionSystem::ray_down_vs_box(
    const Vec3& origin, const WorldPrimitive& box) const
{
    // Луч: p(t) = origin + t * (0,0,-1), t >= 0
    // Преобразуем в систему box
    Eigen::Matrix3d R = rotation_from_pose(box.pose);
    Vec3 box_pos{box.pose.x, box.pose.y, box.pose.z};
    Vec3 local_orig = R.transpose() * (origin - box_pos);
    Vec3 local_dir  = R.transpose() * Vec3{0, 0, -1};

    Vec3 half{box.size.x() / 2.0, box.size.y() / 2.0, box.size.z() / 2.0};

    // Slab method с отслеживанием оси, давшей tmin (для нормали)
    double tmin = -1e18, tmax = 1e18;
    int    tmin_axis = -1;
    double tmin_sign = 1.0;

    for (int i = 0; i < 3; ++i) {
        double d = local_dir[i];
        double o = local_orig[i];
        double h = half[i];

        if (std::abs(d) < 1e-12) {
            if (o < -h || o > h) return std::nullopt;
        } else {
            double t1 = (-h - o) / d;
            double t2 = ( h - o) / d;
            bool swapped = t1 > t2;
            if (swapped) std::swap(t1, t2);
            if (t1 > tmin) {
                tmin = t1;
                tmin_axis = i;
                // Нормаль смотрит от поверхности к источнику луча
                tmin_sign = (local_orig[i] >= 0.0) ? 1.0 : -1.0;
            }
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return std::nullopt;
        }
    }

    if (tmin < 0.0 && tmax < 0.0) return std::nullopt;
    double t = (tmin >= 0.0) ? tmin : tmax;
    if (t < 0.0) return std::nullopt;

    Vec3 hit_local = local_orig + local_dir * t;
    Vec3 hit_world = R * hit_local + box_pos;

    // Нормаль хитовой грани в локальных координатах box
    Vec3 local_normal = Vec3::Zero();
    if (tmin_axis >= 0)
        local_normal[tmin_axis] = tmin_sign;
    else
        local_normal = Vec3{0, 0, 1};  // fallback: верхняя грань

    Vec3 world_normal = (R * local_normal).normalized();
    return RayHit{hit_world.z(), world_normal};
}

// ─── ray_down_vs_cylinder ───────────────────────────────────────────────────

inline std::optional<RayHit> CollisionSystem::ray_down_vs_cylinder(
    const Vec3& origin, const WorldPrimitive& cyl) const
{
    Eigen::Matrix3d R = rotation_from_pose(cyl.pose);
    Vec3 cyl_pos{cyl.pose.x, cyl.pose.y, cyl.pose.z};
    Vec3 lo = R.transpose() * (origin - cyl_pos);
    Vec3 ld = R.transpose() * Vec3{0, 0, -1};

    double half_h = cyl.height / 2.0;
    double r      = cyl.radius;

    std::optional<double> best_t;
    Vec3 best_local_normal{0, 0, 1};

    // Пересечение луча с торцевыми крышками (Z = ±half_h)
    if (std::abs(ld.z()) > 1e-12) {
        for (double cap_z : {half_h, -half_h}) {
            double t = (cap_z - lo.z()) / ld.z();
            if (t < 0.0) continue;
            Vec3 p = lo + ld * t;
            if (p.x() * p.x() + p.y() * p.y() <= r * r) {
                if (!best_t || t < *best_t) {
                    best_t = t;
                    best_local_normal = Vec3{0, 0, (cap_z > 0) ? 1.0 : -1.0};
                }
            }
        }
    }

    // Пересечение луча с боковой поверхностью (XY-круг)
    double ax = ld.x(), ay = ld.y();
    double ox = lo.x(), oy = lo.y();
    double A = ax * ax + ay * ay;
    if (A > 1e-12) {
        double B = 2.0 * (ox * ax + oy * ay);
        double C = ox * ox + oy * oy - r * r;
        double disc = B * B - 4.0 * A * C;
        if (disc >= 0.0) {
            double sq = std::sqrt(disc);
            for (double t : {(-B - sq) / (2.0 * A), (-B + sq) / (2.0 * A)}) {
                if (t < 0.0) continue;
                Vec3 p = lo + ld * t;
                if (std::abs(p.z()) <= half_h) {
                    if (!best_t || t < *best_t) {
                        best_t = t;
                        double rd = std::sqrt(p.x() * p.x() + p.y() * p.y());
                        if (rd > 1e-9)
                            best_local_normal = Vec3{p.x() / rd, p.y() / rd, 0.0};
                        else
                            best_local_normal = Vec3{0, 0, 1};
                    }
                }
            }
        }
    }

    if (!best_t) return std::nullopt;

    Vec3 hit_local = lo + ld * *best_t;
    Vec3 hit_world = R * hit_local + cyl_pos;
    Vec3 world_normal = (R * best_local_normal).normalized();
    return RayHit{hit_world.z(), world_normal};
}

// ─── ray_down_vs_sphere ─────────────────────────────────────────────────────

inline std::optional<RayHit> CollisionSystem::ray_down_vs_sphere(
    const Vec3& origin, const WorldPrimitive& sph) const
{
    Vec3 sph_pos{sph.pose.x, sph.pose.y, sph.pose.z};
    Vec3 oc = origin - sph_pos;
    Vec3 dir{0, 0, -1};
    double B = 2.0 * oc.dot(dir);
    double C = oc.squaredNorm() - sph.radius * sph.radius;
    double disc = B * B - 4.0 * C;

    if (disc < 0.0) return std::nullopt;

    double sq = std::sqrt(disc);
    double t1 = (-B - sq) / 2.0;
    double t2 = (-B + sq) / 2.0;

    double t = -1.0;
    if (t1 >= 0.0) t = t1;
    else if (t2 >= 0.0) t = t2;
    if (t < 0.0) return std::nullopt;

    Vec3 hit = origin + dir * t;
    Vec3 normal = (hit - sph_pos).normalized();
    return RayHit{hit.z(), normal};
}

}  // namespace s2
