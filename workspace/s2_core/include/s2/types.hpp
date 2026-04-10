#pragma once

/**
 * @file types.hpp
 * Базовые типы данных симулятора S2.
 *
 * Для пространственных координат используется Eigen::Vector3d.
 * Причины:
 *  - Eigen — стандарт де-факто в робототехнике и компьютерном зрении
 *  - Больше разработчиков знакомо с Eigen, чем с кастомным Vec3
 *  - Готовые операции: normalize, dot, cross, distance, и т.д.
 *  - SIMD-оптимизация «из коробки»
 *  - Transform3D уже использует Eigen — единый стиль
 *
 * Для тяжёлых вычислений (kinematic tree, матрицы вращения) — Eigen.
 * Для простых координат — тот же Eigen::Vector3d через alias Vec3.
 */
#include <Eigen/Dense>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace s2
{

// ============================================================================
// ID types
// ============================================================================
// Строгая типизация идентификаторов предотвращает случайную путаницу
// между разными сущностями (агент, актор, объект, зона).
// ZoneId — строковый, для читаемости в конфигах и отладке.

/// Уникальный идентификатор агента (робота). Числовой, для скорости.
using AgentId = uint32_t;

/// Уникальный идентификатор актора (дверь, лифт, пешеход).
using ActorId = uint32_t;

/// Уникальный идентификатор пассивного объекта (бочка, ящик, паллета).
using ObjectId = uint32_t;

/// Идентификатор зоны. Строковый — для удобства чтения в YAML-конфигах.
using ZoneId = std::string;

/// Универсальный идентификатор сущности (может указывать на любой тип).
using EntityId = uint32_t;

// ============================================================================
// Spatial types
// ============================================================================

/**
 * @brief Трёхмерный вектор / точка в пространстве.
 *
 * Алиас на Eigen::Vector3d — стандартный тип для координат в робототехнике.
 *
 * Доступ к компонентам: v.x(), v.y(), v.z()
 * Запись: v.x() = 5.0  (Eigen возвращает ссылку через operator())
 *
 * Готовые операции из Eigen:
 *   v1 + v2, v1 - v2, v1 * scalar, scalar * v1
 *   v1.dot(v2), v1.cross(v2)
 *   v1.norm(), v1.squaredNorm(), v1.normalized()
 *   v1.distance(v2)
 *
 * Все значения по умолчанию — нули (Eigen::Vector3d::Zero()).
 */
using Vec3 = Eigen::Vector3d;

/**
 * @brief Полная поза (позиция + ориентация) в 3D пространстве.
 *
 * x, y, z — позиция в мировых координатах.
 * roll, pitch, yaw — углы Эйлера (в радианах).
 *
 * Порядок вращения: ZYX (yaw → pitch → roll).
 *
 * Почему не Eigen::AngleAxisd или кватернион:
 *  - Углы Эйлера — самый понятный формат для конфигов и отладки
 *  - Конвертация в/из кватерниона выполняется при необходимости
 *  - Eigen предоставляет удобные конвертеры: eulerAngles(), AngleAxisd
 */
struct Pose3D
{
  double x{0}, y{0}, z{0};
  double roll{0}, pitch{0}, yaw{0};

  Pose3D() = default;
  Pose3D(double x_, double y_, double z_, double roll_, double pitch_, double yaw_)
      : x(x_), y(y_), z(z_), roll(roll_), pitch(pitch_), yaw(yaw_) {}

  /// Вектор позиции из компонентов позы.
  Vec3 position() const { return Vec3(x, y, z); }

  /// Сравнение с допуском (epsilon = 1e-9 для каждого компонента).
  bool operator==(const Pose3D& other) const
  {
    return std::abs(x - other.x) < 1e-9 && std::abs(y - other.y) < 1e-9 &&
           std::abs(z - other.z) < 1e-9 && std::abs(roll - other.roll) < 1e-9 &&
           std::abs(pitch - other.pitch) < 1e-9 &&
           std::abs(yaw - other.yaw) < 1e-9;
  }

  bool operator!=(const Pose3D& other) const { return !(*this == other); }
};

/**
 * @brief Линейная и угловая скорость.
 *
 * linear — линейная скорость (м/с).
 * angular — угловая скорость (рад/с).
 *
 * Почему Eigen::Vector3d вместо отдельных double:
 *  - Удобные операции: сложение, умножение на скаляр
 *  - Совместимость с координатами (Vec3)
 *  - Меньше кода: v.linear.norm() вместо sqrt(vx*vx + vy*vy + vz*vz)
 */
struct Velocity
{
  Vec3 linear{Vec3::Zero()};   ///< Линейная скорость (vx, vy, vz) в м/с
  Vec3 angular{Vec3::Zero()};  ///< Угловая скорость (wx, wy, wz) в рад/с
};

/**
 * @brief Трансформация в 3D (через Eigen).
 *
 * Используется внутри для kinematic tree, где нужны матрицы вращения.
 * translation — вектор перемещения.
 * rotation — матрица 3x3 (Eigen::Matrix3d).
 */
struct Transform3D
{
  Vec3 translation{Vec3::Zero()};
  Eigen::Matrix3d rotation{Eigen::Matrix3d::Identity()};

  /**
   * @brief Преобразовать точку из локальной системы в мировую.
   * @param local_point Точка в локальной системе координат
   * @return Точка в мировой системе координат
   */
  Vec3 transform_point(const Vec3& local_point) const
  {
    return rotation * local_point + translation;
  }

  /**
   * @brief Преобразовать точку из мировой системы в локальную.
   * @param world_point Точка в мировой системе координат
   * @return Точка в локальной системе координат
   */
  Vec3 inverse_transform_point(const Vec3& world_point) const
  {
    return rotation.transpose() * (world_point - translation);
  }
};

// ============================================================================
// Collision & Visual
// ============================================================================

/**
 * @brief Тип коллизионного примитива.
 *
 * SPHERE — сфера (быстрая проверка столкновений).
 * BOX — ориентированный параллелепипед (AABB).
 * CAPSULE — капсула (цилиндр + полусферы на концах).
 * CYLINDER — цилиндр.
 */
enum class ShapeType
{
  SPHERE,
  BOX,
  CAPSULE,
  CYLINDER
};

/**
 * @brief Описание коллизионного шейпа сущности.
 *
 * Используется для проверки коллизий и для raycast.
 * В зависимости от type используются разные поля:
 *  - SPHERE: radius
 *  - BOX: size (half-extents)
 *  - CAPSULE: radius + height
 *  - CYLINDER: radius + height
 */
struct CollisionShape
{
  ShapeType type{ShapeType::SPHERE};
  double radius{0.5};        ///< Радиус для SPHERE, CAPSULE, CYLINDER
  double height{1.0};        ///< Высота цилиндрической части CAPSULE/CYLINDER
  Vec3 size{1.0, 1.0, 1.0};  ///< Половинные размеры для BOX (half-extents)
};

/**
 * @brief Описание визуального представления сущности.
 *
 * Используется визуализатором для отрисовки.
 */
struct VisualDesc
{
  std::string type{"box"};       ///< "box", "cylinder", "sphere", "capsule"
  Vec3 size{1.0, 1.0, 1.0};      ///< Размеры для box
  double radius{0.5};            ///< Радиус для sphere/cylinder/capsule
  double height{1.0};            ///< Высота для cylinder/capsule
  std::string color{"#FF6B35"};  ///< HEX-цвет
};

// ============================================================================
// Velocity (актуация)
// ============================================================================

/**
 * @brief Желаемая скорость — выход Actuation модуля.
 *
 * Модуль актуации (DiffDrive, Multicopter) вычисляет желаемую скорость.
 * Поле valid = false означает «нет команды» (например, motion_locked).
 *
 * Почему своя структура, а не Velocity:
 *  - Velocity — «текущая скорость», а DesiredVelocity — «желаемая команда»
 *  - Разделение предотвращает случайную запись в состояние вместо команды
 */
struct DesiredVelocity
{
  Vec3 linear{Vec3::Zero()};   ///< Желаемая линейная скорость
  Vec3 angular{Vec3::Zero()};  ///< Желаемая угловая скорость
  bool valid{true};            ///< false = команда игнорируется
};

// ============================================================================
// Zones
// ============================================================================

/**
 * @brief Форма зоны.
 *
 * SPHERE — сфера (центр + радиус).
 * AABB — ориентированный по осям параллелепипед (центр + half-extents).
 * INFINITE — весь мир (для глобальных эффектов: туман, ветер).
 */
enum class ZoneShapeType
{
  SPHERE,
  AABB,
  INFINITE
};

/**
 * @brief Геометрическая форма зоны.
 *
 * Проверяет, находится ли точка внутри зоны.
 * Для SPHERE и AABB граница считается включённой (<=).
 */
struct ZoneShape
{
  ZoneShapeType type{ZoneShapeType::SPHERE};
  Vec3 center{Vec3::Zero()};
  double radius{1.0};             ///< Радиус для SPHERE
  Vec3 half_size{1.0, 1.0, 1.0};  ///< Половинные размеры для AABB

  /**
   * @brief Проверяет, содержится ли точка в данной зоне.
   *
   * SPHERE: squaredNorm до центра <= radius^2 (граница включена).
   *         squaredNorm быстрее norm(), так как нет sqrt.
   *
   * AABB: отклонение по каждой оси <= half_size (граница включена).
   *       Используем cwiseAbs() для поэлементного модуля.
   *
   * INFINITE: всегда true — зона покрывает весь мир.
   *
   * @param point Точка для проверки
   * @return true если точка внутри зоны
   */
  bool contains(const Vec3& point) const
  {
    switch (type)
    {
      case ZoneShapeType::SPHERE: {
        Vec3 diff = point - center;
        return diff.squaredNorm() <= radius * radius;
      }
      case ZoneShapeType::AABB: {
        Vec3 diff = (point - center).cwiseAbs();
        return diff.x() <= half_size.x() && diff.y() <= half_size.y() &&
               diff.z() <= half_size.z();
      }
      case ZoneShapeType::INFINITE:
        return true;
      default:
        return false;
    }
  }
};

// ============================================================================
// Effects
// ============================================================================

/**
 * @brief Тип зонального эффекта.
 *
 * MODIFIER    — модифицирует движение через contribution каждый тик
 *               (лёд, ветер, конвейер). Публикует scale или velocity_addition.
 *
 * CONTINUOUS  — каждый тик изменяет single-owner field в Shared State
 *               (зарядка батареи). Работает пока агент в зоне.
 *
 * MUTATION    — одноразовое необратимое воздействие при входе в зону
 *               (прокол колеса). Не снимается при выходе.
 *
 * SENSOR      — модифицирует параметры конкретного сенсора
 *               (туман → reduce range, помехи → increase noise).
 */
enum class EffectType
{
  MODIFIER,
  CONTINUOUS,
  MUTATION,
  SENSOR
};

// ============================================================================
// Actor state
// ============================================================================

/**
 * @brief Состояние актора (конечный автомат).
 *
 * Примеры: "closed", "opening", "open", "closing" для двери.
 * Тип — строка для простоты; в будущем может быть заменён на enum
 * или отдельную типизированную структуру.
 */
using ActorState = std::string;

}  // namespace s2