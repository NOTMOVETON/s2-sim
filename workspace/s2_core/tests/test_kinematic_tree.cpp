/**
 * @file test_kinematic_tree.cpp
 * Тесты для KinematicTree.
 */

#include <s2/kinematic_tree.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace s2;

// ─── Вспомогательные функции ──────────────────────────────────────────────────

static bool poses_near(const Pose3D& a, const Pose3D& b, double eps = 1e-6)
{
    return std::abs(a.x - b.x) < eps
        && std::abs(a.y - b.y) < eps
        && std::abs(a.z - b.z) < eps
        && std::abs(a.roll  - b.roll)  < eps
        && std::abs(a.pitch - b.pitch) < eps
        && std::abs(a.yaw   - b.yaw)   < eps;
}

// ─── Базовые тесты ─────────────────────────────────────────────────────────────

TEST(KinematicTree, EmptyTree)
{
    KinematicTree tree;
    EXPECT_TRUE(tree.empty());
    EXPECT_EQ(tree.links().size(), 0u);
}

TEST(KinematicTree, SingleLink)
{
    KinematicTree tree;

    Link base;
    base.name   = "base_link";
    base.parent = "";
    base.origin = Pose3D{};
    base.joint.type = JointType::FIXED;
    tree.add_link(base);

    EXPECT_FALSE(tree.empty());
    EXPECT_EQ(tree.links().size(), 1u);
}

// ─── Локальная поза ──────────────────────────────────────────────────────────

TEST(KinematicTree, FixedChildLocalPose)
{
    // base_link → child (смещение x=1, z=0.5)
    KinematicTree tree;

    Link child;
    child.name   = "child";
    child.parent = "base_link";
    child.origin = Pose3D{1.0, 0.0, 0.5, 0.0, 0.0, 0.0};
    child.joint.type = JointType::FIXED;
    tree.add_link(child);

    Pose3D local = tree.compute_local_pose("child");
    Pose3D expected{1.0, 0.0, 0.5, 0.0, 0.0, 0.0};
    EXPECT_TRUE(poses_near(local, expected));
}

TEST(KinematicTree, RevoluteJointLocalPose)
{
    // Вращение вокруг Z на π/2 → point (1,0,0) → (0,1,0)
    KinematicTree tree;

    Link arm;
    arm.name   = "arm";
    arm.parent = "base_link";
    arm.origin = Pose3D{1.0, 0.0, 0.0, 0.0, 0.0, 0.0};  // offset вдоль X
    arm.joint.type  = JointType::REVOLUTE;
    arm.joint.axis  = Vec3{0.0, 0.0, 1.0};
    arm.joint.value = M_PI / 2.0;
    arm.joint.min   = -M_PI;
    arm.joint.max   =  M_PI;
    tree.add_link(arm);

    Pose3D local = tree.compute_local_pose("arm");
    // Offset (1,0,0) + rotate Z PI/2 = (1,0,0) остаётся origin (rotate после offset)
    // yaw должен быть ~π/2
    EXPECT_NEAR(local.x, 1.0, 1e-6);
    EXPECT_NEAR(local.y, 0.0, 1e-6);
    EXPECT_NEAR(local.z, 0.0, 1e-6);
    EXPECT_NEAR(std::abs(local.yaw), M_PI / 2.0, 1e-5);
}

// ─── Мировая поза ────────────────────────────────────────────────────────────

TEST(KinematicTree, WorldPoseRootLink)
{
    // Корневое звено с нулевым origin → его мировая поза = base_pose
    KinematicTree tree;

    Link base;
    base.name   = "base_link";
    base.parent = "";
    base.origin = Pose3D{};
    base.joint.type = JointType::FIXED;
    tree.add_link(base);

    Pose3D base_pose{1.0, 2.0, 0.5, 0.0, 0.0, 1.0};
    // Корень — нет transform к родителю, compute_world_pose вернёт base_pose * T_origin * T_joint
    // Поскольку chain пустая для root (parent пустой), нет трансформации → результат = base_pose
    Pose3D result = tree.compute_world_pose("base_link", base_pose);
    // Результат должен быть base_pose (root добавляет identity через chain)
    EXPECT_NEAR(result.x, 1.0, 1e-5);
    EXPECT_NEAR(result.y, 2.0, 1e-5);
}

TEST(KinematicTree, WorldPoseFixedChild)
{
    // base_link → sensor_link (offset x=0.3, z=0.1, fixed)
    // base_pose = (0,0,0,0,0,0) → world sensor = (0.3, 0, 0.1)
    KinematicTree tree;

    Link sensor;
    sensor.name   = "sensor_link";
    sensor.parent = "base_link";
    sensor.origin = Pose3D{0.3, 0.0, 0.1, 0.0, 0.0, 0.0};
    sensor.joint.type = JointType::FIXED;
    tree.add_link(sensor);

    Pose3D base_pose{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    Pose3D world = tree.compute_world_pose("sensor_link", base_pose);

    EXPECT_NEAR(world.x, 0.3, 1e-6);
    EXPECT_NEAR(world.y, 0.0, 1e-6);
    EXPECT_NEAR(world.z, 0.1, 1e-6);
}

TEST(KinematicTree, WorldPoseChainOfThree)
{
    // base_link → arm1 (x=1, fixed) → camera (x=0.5, fixed)
    // base_pose = (0,0,0) → camera world = (1.5, 0, 0)
    KinematicTree tree;

    Link arm1;
    arm1.name   = "arm1";
    arm1.parent = "base_link";
    arm1.origin = Pose3D{1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    arm1.joint.type = JointType::FIXED;
    tree.add_link(arm1);

    Link camera;
    camera.name   = "camera";
    camera.parent = "arm1";
    camera.origin = Pose3D{0.5, 0.0, 0.0, 0.0, 0.0, 0.0};
    camera.joint.type = JointType::FIXED;
    tree.add_link(camera);

    Pose3D base_pose{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    Pose3D world = tree.compute_world_pose("camera", base_pose);

    EXPECT_NEAR(world.x, 1.5, 1e-5);
    EXPECT_NEAR(world.y, 0.0, 1e-5);
    EXPECT_NEAR(world.z, 0.0, 1e-5);
}

TEST(KinematicTree, WorldPoseWithBasePoseOffset)
{
    // sensor_link в (0.3, 0, 0) от base_link
    // base_pose = (5.0, 0, 0) → world sensor = (5.3, 0, 0)
    KinematicTree tree;

    Link sensor;
    sensor.name   = "sensor_link";
    sensor.parent = "base_link";
    sensor.origin = Pose3D{0.3, 0.0, 0.0, 0.0, 0.0, 0.0};
    sensor.joint.type = JointType::FIXED;
    tree.add_link(sensor);

    Pose3D base_pose{5.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    Pose3D world = tree.compute_world_pose("sensor_link", base_pose);

    EXPECT_NEAR(world.x, 5.3, 1e-5);
    EXPECT_NEAR(world.y, 0.0, 1e-5);
}

// ─── set_joint_value ────────────────────────────────────────────────────────

TEST(KinematicTree, SetJointValueClamped)
{
    KinematicTree tree;

    Link arm;
    arm.name   = "arm";
    arm.parent = "base_link";
    arm.joint.type  = JointType::REVOLUTE;
    arm.joint.axis  = Vec3{0, 0, 1};
    arm.joint.value = 0.0;
    arm.joint.min   = -1.0;
    arm.joint.max   =  1.0;
    tree.add_link(arm);

    tree.set_joint_value("arm", 5.0);   // > max → clamp к 1.0
    EXPECT_NEAR(tree.links()[0].joint.value, 1.0, 1e-9);

    tree.set_joint_value("arm", -5.0);  // < min → clamp к -1.0
    EXPECT_NEAR(tree.links()[0].joint.value, -1.0, 1e-9);

    tree.set_joint_value("arm", 0.5);   // в диапазоне
    EXPECT_NEAR(tree.links()[0].joint.value, 0.5, 1e-9);
}

TEST(KinematicTree, SetJointValueUnknownLink)
{
    KinematicTree tree;
    // Не должен крашиться при неизвестном имени
    EXPECT_NO_THROW(tree.set_joint_value("nonexistent", 1.0));
}

// ─── collect_transforms ──────────────────────────────────────────────────────

TEST(KinematicTree, CollectTransformsOnlyFixed)
{
    // base_link → arm1 (fixed) → camera (fixed)
    // Оба static, dynamic пустой
    KinematicTree tree;

    Link arm1;
    arm1.name = "arm1"; arm1.parent = "base_link";
    arm1.origin = Pose3D{1.0, 0, 0, 0, 0, 0};
    arm1.joint.type = JointType::FIXED;
    tree.add_link(arm1);

    Link cam;
    cam.name = "camera"; cam.parent = "arm1";
    cam.origin = Pose3D{0.5, 0, 0, 0, 0, 0};
    cam.joint.type = JointType::FIXED;
    tree.add_link(cam);

    std::vector<KinematicFrameTransform> stat, dyn;
    tree.collect_transforms(stat, dyn);

    EXPECT_EQ(stat.size(), 2u);
    EXPECT_EQ(dyn.size(), 0u);

    // Проверяем правильность parent/child
    bool found_arm1   = false;
    bool found_camera = false;
    for (const auto& ft : stat)
    {
        if (ft.child_frame == "arm1"   && ft.parent_frame == "base_link") found_arm1   = true;
        if (ft.child_frame == "camera" && ft.parent_frame == "arm1")      found_camera = true;
    }
    EXPECT_TRUE(found_arm1);
    EXPECT_TRUE(found_camera);
}

TEST(KinematicTree, CollectTransformsMixed)
{
    // base_link → revolute_arm (dynamic) → fixed_camera (static)
    KinematicTree tree;

    Link arm;
    arm.name = "arm"; arm.parent = "base_link";
    arm.joint.type = JointType::REVOLUTE;
    arm.joint.axis = Vec3{0, 0, 1};
    tree.add_link(arm);

    Link cam;
    cam.name = "camera"; cam.parent = "arm";
    cam.origin = Pose3D{0.3, 0, 0, 0, 0, 0};
    cam.joint.type = JointType::FIXED;
    tree.add_link(cam);

    std::vector<KinematicFrameTransform> stat, dyn;
    tree.collect_transforms(stat, dyn);

    EXPECT_EQ(stat.size(), 1u);  // camera (fixed)
    EXPECT_EQ(dyn.size(), 1u);   // arm (revolute)

    EXPECT_EQ(dyn[0].child_frame, "arm");
    EXPECT_EQ(stat[0].child_frame, "camera");
}

TEST(KinematicTree, CollectTransformsRootSkipped)
{
    // Корневое звено (parent="") не включается в collect_transforms
    KinematicTree tree;

    Link base;
    base.name = "base_link"; base.parent = "";
    base.joint.type = JointType::FIXED;
    tree.add_link(base);

    std::vector<KinematicFrameTransform> stat, dyn;
    tree.collect_transforms(stat, dyn);

    EXPECT_EQ(stat.size(), 0u);
    EXPECT_EQ(dyn.size(), 0u);
}

// ─── Prismatic joint ─────────────────────────────────────────────────────────

TEST(KinematicTree, PrismaticJoint)
{
    // Линейное смещение вдоль Z
    KinematicTree tree;

    Link lift;
    lift.name   = "lift";
    lift.parent = "base_link";
    lift.origin = Pose3D{};
    lift.joint.type  = JointType::PRISMATIC;
    lift.joint.axis  = Vec3{0.0, 0.0, 1.0};
    lift.joint.value = 0.5;  // поднять на 0.5м
    lift.joint.min   = 0.0;
    lift.joint.max   = 1.0;
    tree.add_link(lift);

    Pose3D base_pose{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    Pose3D world = tree.compute_world_pose("lift", base_pose);

    EXPECT_NEAR(world.z, 0.5, 1e-5);
    EXPECT_NEAR(world.x, 0.0, 1e-5);
}
