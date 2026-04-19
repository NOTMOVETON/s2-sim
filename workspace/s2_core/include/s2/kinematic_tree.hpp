#pragma once

/**
 * @file kinematic_tree.hpp
 * Кинематическое дерево агента.
 *
 * Описывает иерархию звеньев (links) и джоинтов (joints) тела агента.
 * Используется для:
 *  - Вычисления мировых поз всех звеньев (форвардная кинематика)
 *  - Генерации TF-трансформов для транспортного слоя
 *
 * Транспортный слой НЕ видит KinematicTree напрямую.
 * SimTransportBridge вызывает collect_transforms() и передаёт готовые
 * FrameTransform в ITransportAdapter.
 */

#include <s2/types.hpp>

#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>

namespace s2
{

// Forward declaration (FrameTransform определён в transport_adapter.hpp,
// но чтобы избежать циклической зависимости — определим здесь локально).
// SimTransportBridge преобразует KinematicFrameTransform в FrameTransform.

/**
 * @brief Трансформ между двумя фреймами кинематического дерева.
 * Дублирует FrameTransform из transport_adapter.hpp, но без зависимости на него.
 * SimTransportBridge копирует поля напрямую.
 */
struct KinematicFrameTransform
{
    std::string parent_frame;
    std::string child_frame;
    Pose3D      relative_pose;
    bool        is_static{false};  ///< true = fixed joint (публиковать один раз)
};

// ============================================================================
// Joint
// ============================================================================

/**
 * @brief Тип джоинта.
 *
 * FIXED      — жёстко закреплён, relative_pose не меняется
 * REVOLUTE   — вращение вокруг оси (угол в радианах, ограничен [min, max])
 * PRISMATIC  — трансляция вдоль оси (метры, ограничен [min, max])
 * CONTINUOUS — вращение без ограничений (полный оборот)
 */
enum class JointType
{
    FIXED,
    REVOLUTE,
    PRISMATIC,
    CONTINUOUS,
};

/**
 * @brief Джоинт, соединяющий звено с родителем.
 */
struct Joint
{
    JointType type{JointType::FIXED};

    Vec3   axis{0.0, 0.0, 1.0};  ///< Ось вращения/трансляции (нормализована)
    double value{0.0};           ///< Текущее значение (рад или м)
    double min{-M_PI};           ///< Минимальное значение (для REVOLUTE/PRISMATIC)
    double max{ M_PI};           ///< Максимальное значение

    /// Является ли джоинт статичным (значение не меняется)
    bool is_static() const
    {
        return type == JointType::FIXED;
    }
};

// ============================================================================
// Link
// ============================================================================

/**
 * @brief Звено кинематического дерева.
 *
 * Каждое звено имеет:
 *  - имя (уникальное в дереве)
 *  - родительское звено (пустое для корня — base_link)
 *  - origin: смещение от родителя при value == 0 (offset + ориентация)
 *  - joint: тип соединения с родителем
 */
/**
 * @brief Визуальная геометрия звена (из <visual> в URDF).
 * type == "" означает отсутствие визуальной геометрии.
 */
struct LinkVisual
{
    std::string type;           ///< "box", "cylinder", "sphere" или "" (нет геометрии)
    // box
    double sx{1.0}, sy{1.0}, sz{1.0};
    // cylinder / sphere
    double radius{0.5}, length{1.0};
    // цвет в формате "#RRGGBB"
    std::string color{"#888888"};
    // смещение визуальной геометрии от фрейма звена
    Pose3D origin;
};

struct Link
{
    std::string name;    ///< Уникальное имя, например "arm_link_1", "camera_link"
    std::string parent;  ///< Имя родителя, "" для корневого звена

    Pose3D     origin;  ///< Смещение и ориентация относительно родителя (при value=0)
    Joint      joint;   ///< Джоинт, соединяющий с родителем
    LinkVisual visual;  ///< Визуальная геометрия звена (из URDF <visual>)
};

// ============================================================================
// KinematicTree
// ============================================================================

/**
 * @brief Кинематическое дерево агента.
 *
 * Хранит иерархию звеньев и позволяет:
 *  - Вычислять мировую позу любого звена по базовой позе агента
 *  - Сплющивать дерево в список FrameTransform для транспортного слоя
 *
 * Корневое звено обычно называется "base_link" и является точкой отсчёта.
 * Дерево может не содержать base_link явно — тогда корнем считается
 * первое добавленное звено с пустым parent.
 */
class KinematicTree
{
public:
    /**
     * @brief Добавить звено в дерево.
     * Порядок добавления не важен (родитель может быть добавлен позже),
     * но при вычислениях дерево должно быть связным.
     */
    void add_link(Link link);

    bool empty() const { return links_.empty(); }

    const std::vector<Link>& links() const { return links_; }

    /**
     * @brief Установить текущее значение джоинта для звена.
     * @param link_name  Имя звена, чей джоинт нужно обновить
     * @param value      Новое значение (рад для REVOLUTE/CONTINUOUS, м для PRISMATIC)
     */
    void set_joint_value(const std::string& link_name, double value);

    /**
     * @brief Установить цвет визуальной геометрии звена.
     * @param link_name  Имя звена
     * @param color      Цвет в формате "#RRGGBB" или CSS-имя
     */
    void set_link_color(const std::string& link_name, const std::string& color);

    /**
     * @brief Вычислить мировую позу звена по базовой позе агента.
     *
     * Обходит цепочку от link_name до корня, применяя трансформы джоинтов.
     * Стоимость: O(depth).
     *
     * @param link_name  Имя целевого звена
     * @param base_pose  Мировая поза корневого звена (base_link)
     * @return Мировая поза целевого звена
     */
    Pose3D compute_world_pose(const std::string& link_name,
                              const Pose3D& base_pose) const;

    /**
     * @brief Вычислить относительную позу звена (относительно родителя).
     * Учитывает текущее значение джоинта.
     *
     * @param link_name  Имя звена
     * @return Поза звена в системе координат родителя
     */
    Pose3D compute_local_pose(const std::string& link_name) const;

    /**
     * @brief Сплющить дерево в два списка трансформов для транспортного слоя.
     *
     * Статические (FIXED джоинты) → out_static  (регистрировать один раз)
     * Динамические                 → out_dynamic (публиковать каждый тик)
     *
     * Трансформы описывают отношение parent→child с текущим значением джоинта.
     *
     * @param out_static   Выходной список для статических трансформов
     * @param out_dynamic  Выходной список для динамических трансформов
     */
    void collect_transforms(std::vector<KinematicFrameTransform>& out_static,
                            std::vector<KinematicFrameTransform>& out_dynamic) const;

private:
    std::vector<Link> links_;
    std::unordered_map<std::string, size_t> link_index_;  ///< имя → индекс в links_
};

}  // namespace s2
