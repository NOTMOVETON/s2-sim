/**
 * @file kinematic_tree.cpp
 * Реализация KinematicTree.
 */

#include <s2/kinematic_tree.hpp>

#include <Eigen/Geometry>
#include <stdexcept>
#include <cmath>

namespace s2
{

// ─── Helpers ─────────────────────────────────────────────────────────────────

/// Конвертировать Pose3D в Eigen::Isometry3d (Translation + ZYX Euler)
static Eigen::Isometry3d pose_to_isometry(const Pose3D& p)
{
    Eigen::Isometry3d T = Eigen::Isometry3d::Identity();
    T.translation() = Eigen::Vector3d(p.x, p.y, p.z);
    T.linear() =
        (Eigen::AngleAxisd(p.yaw,   Eigen::Vector3d::UnitZ())
       * Eigen::AngleAxisd(p.pitch, Eigen::Vector3d::UnitY())
       * Eigen::AngleAxisd(p.roll,  Eigen::Vector3d::UnitX())).toRotationMatrix();
    return T;
}

/// Конвертировать Eigen::Isometry3d обратно в Pose3D (ZYX Euler)
/// Корректное разложение матрицы вращения в yaw/pitch/roll с диапазоном [-π, π].
/// Eigen::eulerAngles() НЕ гарантирует диапазон — нормализуем вручную.
static Pose3D isometry_to_pose(const Eigen::Isometry3d& T)
{
    Pose3D p;
    p.x = T.translation().x();
    p.y = T.translation().y();
    p.z = T.translation().z();

    // Используем eulerAngles(2,1,0) = yaw, pitch, roll, затем нормализуем
    // в диапазон [-π, π], чтобы избежать скачков при ±180°
    Eigen::Vector3d euler = T.linear().eulerAngles(2, 1, 0);
    p.yaw   = std::atan2(std::sin(euler[0]), std::cos(euler[0]));
    p.pitch = std::atan2(std::sin(euler[1]), std::cos(euler[1]));
    p.roll  = std::atan2(std::sin(euler[2]), std::cos(euler[2]));
    return p;
}

/// Вычислить трансформ джоинта: смещение/поворот, вносимый джоинтом при текущем value
static Eigen::Isometry3d joint_transform(const Joint& joint)
{
    Eigen::Isometry3d T = Eigen::Isometry3d::Identity();

    switch (joint.type)
    {
        case JointType::FIXED:
            break;  // identity

        case JointType::REVOLUTE:
        case JointType::CONTINUOUS:
        {
            // Вращение вокруг оси на текущий угол
            Eigen::Vector3d axis = joint.axis.normalized();
            T.linear() = Eigen::AngleAxisd(joint.value, axis).toRotationMatrix();
            break;
        }

        case JointType::PRISMATIC:
        {
            // Трансляция вдоль оси
            Eigen::Vector3d axis = joint.axis.normalized();
            T.translation() = axis * joint.value;
            break;
        }
    }

    return T;
}

// ─── KinematicTree ────────────────────────────────────────────────────────────

void KinematicTree::add_link(Link link)
{
    const std::string name = link.name;
    links_.push_back(std::move(link));
    link_index_[name] = links_.size() - 1;
}

void KinematicTree::set_joint_value(const std::string& link_name, double value)
{
    auto it = link_index_.find(link_name);
    if (it == link_index_.end())
        return;  // неизвестное звено — игнорируем

    auto& joint = links_[it->second].joint;

    // Ограничиваем значение для REVOLUTE и PRISMATIC
    if (joint.type == JointType::REVOLUTE || joint.type == JointType::PRISMATIC)
        value = std::clamp(value, joint.min, joint.max);

    joint.value = value;
}

void KinematicTree::set_link_color(const std::string& link_name, const std::string& color)
{
    auto it = link_index_.find(link_name);
    if (it == link_index_.end())
        return;
    links_[it->second].visual.color = color;
}

Pose3D KinematicTree::compute_local_pose(const std::string& link_name) const
{
    auto it = link_index_.find(link_name);
    if (it == link_index_.end())
        return Pose3D{};

    const Link& link = links_[it->second];

    // T_local = T_origin * T_joint
    Eigen::Isometry3d T_origin = pose_to_isometry(link.origin);
    Eigen::Isometry3d T_joint  = joint_transform(link.joint);
    Eigen::Isometry3d T_local  = T_origin * T_joint;

    return isometry_to_pose(T_local);
}

Pose3D KinematicTree::compute_world_pose(const std::string& link_name,
                                         const Pose3D& base_pose) const
{
    // Собираем цепочку от link_name до корня
    std::vector<const Link*> chain;

    std::string current = link_name;
    while (!current.empty())
    {
        auto it = link_index_.find(current);
        if (it == link_index_.end())
            break;

        const Link* lptr = &links_[it->second];
        chain.push_back(lptr);
        current = lptr->parent;
    }

    // Строим глобальный трансформ: T_world = T_base * T_chain[N-1] * ... * T_chain[0]
    Eigen::Isometry3d T_world = pose_to_isometry(base_pose);

    // chain[0] = link_name, chain.back() = первое звено цепочки (дочернее корня)
    // Применяем в обратном порядке
    for (int i = static_cast<int>(chain.size()) - 1; i >= 0; --i)
    {
        const Link* lptr = chain[static_cast<size_t>(i)];
        Eigen::Isometry3d T_origin = pose_to_isometry(lptr->origin);
        Eigen::Isometry3d T_joint  = joint_transform(lptr->joint);
        T_world = T_world * T_origin * T_joint;
    }

    return isometry_to_pose(T_world);
}

void KinematicTree::collect_transforms(
    std::vector<KinematicFrameTransform>& out_static,
    std::vector<KinematicFrameTransform>& out_dynamic) const
{
    for (const auto& link : links_)
    {
        if (link.parent.empty())
            continue;  // корневое звено — нет трансформа к родителю

        KinematicFrameTransform ft;
        ft.parent_frame = link.parent;
        ft.child_frame  = link.name;
        ft.relative_pose = compute_local_pose(link.name);
        ft.is_static     = link.joint.is_static();

        if (ft.is_static)
            out_static.push_back(ft);
        else
            out_dynamic.push_back(ft);
    }
}

}  // namespace s2
