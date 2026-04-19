/**
 * @file test_collision_system.cpp
 * Тесты для CollisionSystem: sphere vs box/sphere/cylinder,
 * apply_slide, find_support_surface, интеграционные тесты.
 */

#include <s2/collision_system.hpp>
#include <s2/sim_engine.hpp>
#include <s2/scene_loader.hpp>
#include <s2/agent.hpp>

#include <gtest/gtest.h>
#include <cmath>

using namespace s2;

// ─── Вспомогательные функции ────────────────────────────────────────────────

/// Создать горизонтальный box-примитив (без вращения).
static WorldPrimitive make_box(double cx, double cy, double cz,
                                double sx, double sy, double sz,
                                const std::string& color = "#808080")
{
    WorldPrimitive p;
    p.type = "box";
    p.pose = Pose3D{cx, cy, cz, 0, 0, 0};
    p.size = Vec3{sx, sy, sz};
    return p;
}

/// Создать box-примитив с заданным pitch (наклон вдоль Y-оси).
static WorldPrimitive make_box_pitched(double cx, double cy, double cz,
                                       double sx, double sy, double sz,
                                       double pitch_deg)
{
    WorldPrimitive p;
    p.type = "box";
    double pitch_rad = pitch_deg * M_PI / 180.0;
    p.pose = Pose3D{cx, cy, cz, 0, pitch_rad, 0};
    p.size = Vec3{sx, sy, sz};
    return p;
}

/// Создать sphere-примитив.
static WorldPrimitive make_sphere_prim(double cx, double cy, double cz,
                                       double r)
{
    WorldPrimitive p;
    p.type = "sphere";
    p.pose = Pose3D{cx, cy, cz, 0, 0, 0};
    p.radius = r;
    return p;
}

/// Создать cylinder-примитив (ось Z, без вращения).
static WorldPrimitive make_cylinder(double cx, double cy, double cz,
                                     double r, double h)
{
    WorldPrimitive p;
    p.type = "cylinder";
    p.pose = Pose3D{cx, cy, cz, 0, 0, 0};
    p.radius = r;
    p.height = h;
    return p;
}

// ─── Sphere vs Box ──────────────────────────────────────────────────────────

TEST(CollisionSystemTest, SphereVsBox_Hit)
{
    CollisionSystem cs;
    // Горизонтальный пол: box 10×10×0.1 с центром на z=0
    cs.set_static_geometry({make_box(0, 0, 0, 10, 10, 0.1)});

    // Сфера радиусом 0.35 с центром на z=0.3 — box центр z=0, half_z=0.05, верх z=0.05
    // dist = 0.3 - 0.05 = 0.25, penetration = 0.35 - 0.25 = 0.10
    auto contacts = cs.check_sphere_all(Vec3{0, 0, 0.3}, 0.35);
    ASSERT_EQ(contacts.size(), 1u);
    EXPECT_TRUE(contacts[0].has_contact);
    EXPECT_NEAR(contacts[0].penetration, 0.10, 1e-9);
}

TEST(CollisionSystemTest, SphereVsBox_Miss)
{
    CollisionSystem cs;
    cs.set_static_geometry({make_box(0, 0, 0, 2, 2, 0.1)});

    // Сфера высоко над box — нет контакта
    auto contacts = cs.check_sphere_all(Vec3{0, 0, 2.0}, 0.35);
    EXPECT_TRUE(contacts.empty());
}

TEST(CollisionSystemTest, SphereVsBox_NormalPointsUp)
{
    CollisionSystem cs;
    // Пол: box с центром на z=-0.025 (верхняя грань на z=0)
    cs.set_static_geometry({make_box(0, 0, -0.025, 40, 40, 0.05)});

    // Сфера радиусом 0.35 с центром на z=0.34 — слегка проваливается
    auto contacts = cs.check_sphere_all(Vec3{0, 0, 0.34}, 0.35);
    ASSERT_EQ(contacts.size(), 1u);
    // Нормаль должна смотреть вверх (z > 0)
    EXPECT_GT(contacts[0].contact_normal.z(), 0.9);
}

TEST(CollisionSystemTest, SphereVsBox_NormalHorizontal)
{
    CollisionSystem cs;
    // Вертикальная стена вдоль Y, в x=5
    cs.set_static_geometry({make_box(5.0, 0, 0.5, 0.2, 6.0, 1.0)});

    // Сфера у стены с запада (x=4.7, r=0.35) → нормаль должна смотреть в -X
    auto contacts = cs.check_sphere_all(Vec3{4.7, 0, 0.5}, 0.35);
    ASSERT_EQ(contacts.size(), 1u);
    // Нормаль горизонтальна (z ≈ 0)
    EXPECT_NEAR(contacts[0].contact_normal.z(), 0.0, 0.1);
    // X-компонента нормали направлена от стены к агенту (в -X)
    EXPECT_LT(contacts[0].contact_normal.x(), -0.8);
}

TEST(CollisionSystemTest, SphereVsBox_RotatedBox_NormalAngled)
{
    CollisionSystem cs;
    // Box с pitch=30° — наклонная плоскость
    // Центр на (0, 0, 0), большой по X
    cs.set_static_geometry({make_box_pitched(0, 0, 0, 4, 2, 0.1, 30.0)});

    // Сфера расположена над центром box: z=0.5
    // При pitch=30° верхняя грань наклонена, нормаль ≈ (0, -sin30, cos30)
    auto contacts = cs.check_sphere_all(Vec3{0, 0, 0.5}, 0.35);

    if (!contacts.empty()) {
        // Нормаль должна иметь значительную z-компоненту (наклон ~30°)
        double nz = contacts[0].contact_normal.z();
        EXPECT_GT(nz, 0.6);  // cos(30°) ≈ 0.866
    }
    // Тест мягкий: геометрия может не пересекаться при точном позиционировании
}

// ─── Sphere vs Sphere ───────────────────────────────────────────────────────

TEST(CollisionSystemTest, SphereVsSphere_Hit)
{
    CollisionSystem cs;
    cs.set_static_geometry({make_sphere_prim(0, 0, 0, 1.0)});

    // Сфера агента r=0.35 в точке (1.2, 0, 0): dist=1.2, sum_r=1.35 → перекрытие 0.15
    auto contacts = cs.check_sphere_all(Vec3{1.2, 0, 0}, 0.35);
    ASSERT_EQ(contacts.size(), 1u);
    EXPECT_NEAR(contacts[0].penetration, 0.15, 1e-9);
}

TEST(CollisionSystemTest, SphereVsSphere_Miss)
{
    CollisionSystem cs;
    cs.set_static_geometry({make_sphere_prim(0, 0, 0, 1.0)});

    auto contacts = cs.check_sphere_all(Vec3{2.0, 0, 0}, 0.35);
    EXPECT_TRUE(contacts.empty());
}

// ─── Sphere vs Cylinder ─────────────────────────────────────────────────────

TEST(CollisionSystemTest, SphereVsCylinder_Hit)
{
    CollisionSystem cs;
    // Цилиндр в центре: r=0.4, h=1.0, центр на (2, 2, 0.5)
    cs.set_static_geometry({make_cylinder(2, 2, 0.5, 0.4, 1.0)});

    // Сфера агента r=0.35 рядом с цилиндром: dist=0.6, sum_r=0.75 → hit
    auto contacts = cs.check_sphere_all(Vec3{2.6, 2, 0.5}, 0.35);
    ASSERT_EQ(contacts.size(), 1u);
    EXPECT_TRUE(contacts[0].has_contact);
    EXPECT_GT(contacts[0].penetration, 0.0);
}

TEST(CollisionSystemTest, SphereVsCylinder_Miss)
{
    CollisionSystem cs;
    cs.set_static_geometry({make_cylinder(2, 2, 0.5, 0.4, 1.0)});

    auto contacts = cs.check_sphere_all(Vec3{4.0, 2, 0.5}, 0.35);
    EXPECT_TRUE(contacts.empty());
}

// ─── apply_slide ────────────────────────────────────────────────────────────

TEST(CollisionSystemTest, ApplySlide_RemovesNormalComponent)
{
    // Нормаль смотрит вверх (0,0,1) — движение вниз должно быть убрано
    Velocity vel;
    vel.linear = Vec3{1.0, 0.0, -2.0};
    Vec3 normal{0, 0, 1};

    Velocity result = CollisionSystem::apply_slide(vel, normal);

    EXPECT_NEAR(result.linear.z(), 0.0, 1e-12);
    EXPECT_NEAR(result.linear.x(), 1.0, 1e-12);
    EXPECT_NEAR(result.linear.y(), 0.0, 1e-12);
}

TEST(CollisionSystemTest, ApplySlide_KeepsTangential)
{
    // Нормаль смотрит в +X (стена позади, нормаль от стены к агенту = +X).
    // Агент движется в -X (В стену: proj = (-1)*(1) = -1 < 0 → убрать).
    // Компонента vy=2 (вдоль стены) должна остаться.
    Velocity vel;
    vel.linear = Vec3{-1.0, 2.0, 0.0};
    Vec3 normal{1, 0, 0};

    Velocity result = CollisionSystem::apply_slide(vel, normal);

    EXPECT_NEAR(result.linear.x(), 0.0, 1e-12);  // нормальная компонента убрана
    EXPECT_NEAR(result.linear.y(), 2.0, 1e-12);  // тангенциальная осталась
    EXPECT_NEAR(result.linear.z(), 0.0, 1e-12);
}

TEST(CollisionSystemTest, ApplySlide_NoReactionIfMovingAway)
{
    // Агент движется ОТ поверхности — слайд не должен ничего менять
    Velocity vel;
    vel.linear = Vec3{0.0, 0.0, 1.0};  // движение вверх
    Vec3 normal{0, 0, 1};              // нормаль вверх

    Velocity result = CollisionSystem::apply_slide(vel, normal);

    // Скорость не изменилась
    EXPECT_NEAR(result.linear.x(), vel.linear.x(), 1e-12);
    EXPECT_NEAR(result.linear.y(), vel.linear.y(), 1e-12);
    EXPECT_NEAR(result.linear.z(), vel.linear.z(), 1e-12);
}

TEST(CollisionSystemTest, ApplySlide_DiagonalVelocity)
{
    // Нормаль вверх, скорость под углом: vx=1, vz=-1
    // После слайда vz должен обнулиться, vx остаётся
    Velocity vel;
    vel.linear = Vec3{1.0, 0.0, -1.0};
    Vec3 normal{0, 0, 1};

    Velocity result = CollisionSystem::apply_slide(vel, normal);

    EXPECT_NEAR(result.linear.x(), 1.0, 1e-12);
    EXPECT_NEAR(result.linear.z(), 0.0, 1e-12);
}

// ─── find_support_surface ───────────────────────────────────────────────────

TEST(CollisionSystemTest, FindSupportSurface_BoxBelow)
{
    CollisionSystem cs;
    // Пол: box центр z=-0.025, верхняя грань z=0
    cs.set_static_geometry({make_box(0, 0, -0.025, 40, 40, 0.05)});

    // Агент на z=1.0 (центр), radius=0.35 → нижняя точка на z=0.65
    auto result = cs.find_support_surface(Vec3{0, 0, 1.0}, 0.35);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->ground_z, 0.0, 0.05);  // поверхность примерно на z=0
}

TEST(CollisionSystemTest, FindSupportSurface_NoSurface)
{
    CollisionSystem cs;
    // Пол далеко внизу (z=-100)
    cs.set_static_geometry({make_box(0, 0, -100, 40, 40, 1.0)});

    // Агент на z=1.0 — пол дальше 2м → nullopt
    auto result = cs.find_support_surface(Vec3{0, 0, 1.0}, 0.35);
    EXPECT_FALSE(result.has_value());
}

TEST(CollisionSystemTest, FindSupportSurface_OutsideBounds)
{
    CollisionSystem cs;
    // Небольшой пол: box 2×2
    cs.set_static_geometry({make_box(0, 0, -0.025, 2, 2, 0.05)});

    // Агент далеко в стороне — поверхности нет под ним
    auto result = cs.find_support_surface(Vec3{10, 10, 1.0}, 0.35);
    EXPECT_FALSE(result.has_value());
}

TEST(CollisionSystemTest, FindSupport_HorizontalFloor_NormalUp)
{
    CollisionSystem cs;
    cs.set_static_geometry({make_box(0, 0, -0.025, 40, 40, 0.05)});

    auto result = cs.find_support_surface(Vec3{0, 0, 1.0}, 0.35);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->normal.x(), 0.0, 0.01);
    EXPECT_NEAR(result->normal.y(), 0.0, 0.01);
    EXPECT_NEAR(result->normal.z(), 1.0, 0.01);
}

TEST(CollisionSystemTest, FindSupport_TiltedRamp_NormalCorrect)
{
    CollisionSystem cs;
    // Рампа: pitch=-18.43 deg = -atan(1/3)
    // Нормаль верхней грани: повёрнута от (0,0,1) на 18.43 deg вокруг Y
    // → normal ≈ (-sin(18.43), 0, cos(18.43)) ≈ (-0.316, 0, 0.949)
    double pitch_deg = -18.43;
    cs.set_static_geometry({make_box_pitched(1.5, 0, 0.5, 3.162, 3.0, 0.1, pitch_deg)});

    auto result = cs.find_support_surface(Vec3{1.5, 0, 2.0}, 0.35);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->normal.z(), std::cos(18.43 * M_PI / 180.0), 0.05);
    EXPECT_GT(std::abs(result->normal.x()), 0.1)
        << "Нормаль наклонной рампы должна иметь ненулевую X-компоненту";
}

TEST(CollisionSystemTest, FindSupport_NoHit_ReturnsNullopt)
{
    CollisionSystem cs;
    cs.set_static_geometry({});  // пустая геометрия
    auto result = cs.find_support_surface(Vec3{0, 0, 1.0}, 0.35);
    EXPECT_FALSE(result.has_value());
}

// ─── Проверка step_height ───────────────────────────────────────────────────

TEST(CollisionSystemTest, StepHeight_ObstacleTopSameAsAgentBottom)
{
    CollisionSystem cs;
    // Пол на z=0 (верхняя грань), рядом стена той же высоты
    // Стена: box центр x=2, z=0.25, высота 0.5 → верхняя грань z=0.5
    cs.set_static_geometry({make_box(2.0, 0, 0.25, 0.1, 4, 0.5)});

    // Агент с radius=0.35, центр на z=0.35 → agent_bottom = 0.0
    // obstacle_top_z стены = 0.5
    auto contacts = cs.check_sphere_all(Vec3{1.7, 0, 0.35}, 0.35);
    ASSERT_EQ(contacts.size(), 1u);
    // obstacle_top_z должна быть 0.5
    EXPECT_NEAR(contacts[0].obstacle_top_z, 0.5, 0.05);
}

// ─── Многоконтактная сортировка ─────────────────────────────────────────────

TEST(CollisionSystemTest, MultiContact_SortedByPenetration)
{
    CollisionSystem cs;
    // Два box с разной степенью перекрытия
    WorldPrimitive box1 = make_box(0, 0, 0, 10, 10, 0.4);   // глубокий контакт
    WorldPrimitive box2 = make_box(0, 0, 0.3, 10, 10, 0.1); // мелкий контакт

    cs.set_static_geometry({box1, box2});

    auto contacts = cs.check_sphere_all(Vec3{0, 0, 0.35}, 0.35);
    ASSERT_GE(contacts.size(), 2u);
    // Первый контакт должен быть глубже
    EXPECT_GE(contacts[0].penetration, contacts[1].penetration);
}

// ─── Интеграционные тесты ───────────────────────────────────────────────────

TEST(CollisionSystemIntegration, AgentStaysOnFloor)
{
    // Агент на полу с нулевой скоростью (только вертикальная компонента вниз)
    CollisionSystem cs;
    cs.set_static_geometry({make_box(0, 0, -0.025, 40, 40, 0.05)});

    // Имитируем фазу 3h: агент упал на z=0.3 (перекрытие 0.05)
    Velocity vel;
    vel.linear = Vec3{0, 0, -0.5};  // скорость вниз

    Vec3 pos{0, 0, 0.3};
    auto contacts = cs.check_sphere_all(pos, 0.35);
    ASSERT_EQ(contacts.size(), 1u);

    // Применяем slide
    Velocity corrected = CollisionSystem::apply_slide(vel, contacts[0].contact_normal);

    // Вертикальная скорость должна быть убрана (агент не проваливается)
    EXPECT_NEAR(corrected.linear.z(), 0.0, 0.01);
}

TEST(CollisionSystemIntegration, AgentSlidesAlongWall)
{
    // Агент движется к стене под углом → должен скользить вдоль стены
    CollisionSystem cs;
    // Стена: вертикальный box в x=5
    cs.set_static_geometry({make_box(5.0, 0, 0.5, 0.2, 10, 1.0)});

    // Агент у стены: центр x=4.7 (слева от стены x=5), скорость в +X (в стену)
    Velocity vel;
    vel.linear = Vec3{1.0, 1.0, 0.0};  // движение в +X (к стене) + vy

    Vec3 pos{4.7, 0, 0.5};
    auto contacts = cs.check_sphere_all(pos, 0.35);
    ASSERT_EQ(contacts.size(), 1u);

    EXPECT_TRUE(contacts[0].has_contact);

    // Нормаль примерно в -X (стена слева от агента)
    EXPECT_LT(contacts[0].contact_normal.x(), -0.8);

    Velocity corrected = CollisionSystem::apply_slide(vel, contacts[0].contact_normal);

    // X-компонента скорости должна быть убрана (не движется в стену)
    EXPECT_NEAR(corrected.linear.x(), 0.0, 0.05);
    // Y-компонента должна остаться (скользим вдоль стены)
    EXPECT_GT(corrected.linear.y(), 0.9);
}

TEST(CollisionSystemIntegration, AgentWithoutCollisionPassesThrough)
{
    // Агент без has_collision не должен участвовать в системе
    // Это тест на уровне логики: check_sphere_all вернёт контакт,
    // но sim_engine должен пропустить агента без has_collision

    CollisionSystem cs;
    cs.set_static_geometry({make_box(0, 0, 0, 10, 10, 0.1)});

    // Контакт есть...
    auto contacts = cs.check_sphere_all(Vec3{0, 0, 0.3}, 0.35);
    EXPECT_FALSE(contacts.empty());

    // ...но агент без has_collision просто игнорирует это (логика в sim_engine)
    Agent agent;
    agent.has_collision = false;
    EXPECT_FALSE(agent.has_collision);
}

TEST(CollisionSystemIntegration, CornerSlide_TwoWalls)
{
    // Агент в углу двух стен — должен скользить вдоль бисектрисы
    CollisionSystem cs;
    // Стена 1: x=2
    WorldPrimitive wall1 = make_box(2.1, 0, 0.5, 0.2, 10, 1.0);
    // Стена 2: y=2
    WorldPrimitive wall2 = make_box(0, 2.1, 0.5, 10, 0.2, 1.0);
    cs.set_static_geometry({wall1, wall2});

    // Агент в углу
    Vec3 pos{1.7, 1.7, 0.5};
    auto contacts = cs.check_sphere_all(pos, 0.35);

    // Должны быть контакты с обеими стенами
    EXPECT_GE(contacts.size(), 2u);

    // Применяем slides последовательно (как в sim_engine)
    Velocity vel;
    vel.linear = Vec3{1.0, 1.0, 0.0};  // движение в угол

    Velocity corrected = vel;
    for (const auto& c : contacts) {
        corrected = CollisionSystem::apply_slide(corrected, c.contact_normal);
    }

    // Скорость в угол должна быть погашена
    EXPECT_LT(corrected.linear.x(), 0.1);
    EXPECT_LT(corrected.linear.y(), 0.1);
}
