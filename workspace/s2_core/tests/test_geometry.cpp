#include <gtest/gtest.h>
#include <s2/heightmap.hpp>
#include <s2/raycast_engine.hpp>
#include <s2/world.hpp>

namespace s2 {

// ─── Heightmap Tests ─────────────────────────────────────────────

TEST(Heightmap, FlatHeightAt) {
    auto hm = Heightmap::flat(20.0, 20.0, 0.0);
    EXPECT_DOUBLE_EQ(hm.height_at(0.0, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(hm.height_at(5.0, 5.0), 0.0);
    EXPECT_DOUBLE_EQ(hm.height_at(-10.0, 10.0), 0.0);
}

TEST(Heightmap, FlatNonZeroZ) {
    auto hm = Heightmap::flat(20.0, 20.0, 1.5);
    EXPECT_DOUBLE_EQ(hm.height_at(0.0, 0.0), 1.5);
    EXPECT_DOUBLE_EQ(hm.height_at(999.0, 999.0), 1.5);  // вне границ всё равно отдаёт Z
}

TEST(Heightmap, NormalAt) {
    auto hm = Heightmap::flat(20.0, 20.0, 0.0);
    Vec3 n = hm.normal_at(5.0, 5.0);
    EXPECT_DOUBLE_EQ(n.x(), 0.0);
    EXPECT_DOUBLE_EQ(n.y(), 0.0);
    EXPECT_DOUBLE_EQ(n.z(), 1.0);
}

TEST(Heightmap, InBounds) {
    auto hm = Heightmap::flat(20.0, 20.0, 0.0);
    // Центр на (0,0), так что границы -10..10
    EXPECT_TRUE(hm.in_bounds(0.0, 0.0));
    EXPECT_TRUE(hm.in_bounds(5.0, 5.0));
    EXPECT_TRUE(hm.in_bounds(10.0, 10.0));
    EXPECT_FALSE(hm.in_bounds(100.0, 0.0));
    EXPECT_FALSE(hm.in_bounds(0.0, 100.0));
}

TEST(Heightmap, WidthHeight) {
    auto hm = Heightmap::flat(30.0, 15.0, 0.0);
    EXPECT_DOUBLE_EQ(hm.width(), 30.0);
    EXPECT_DOUBLE_EQ(hm.height(), 15.0);
}

// ─── Raycast Engine Tests ────────────────────────────────────────

TEST(RayCast, RayMisses) {
    RaycastEngine engine;

    // Пустой мир — все лучи промахиваются
    Ray ray{Vec3{0, 0, 0}, Vec3{1, 0, 0}, 30.0};
    auto result = engine.cast(ray);
    EXPECT_FALSE(result.hit);
}

TEST(RayCast, RayHitsBox) {
    RaycastEngine engine;

    WorldPrimitive wall;
    wall.type = "box";
    wall.pose.x = 10.0;
    wall.pose.y = 0.0;
    wall.pose.z = 1.0;
    wall.size = Vec3{0.2, 10.0, 2.0};
    engine.set_static_geometry({wall});

    // Луч к стене
    Ray ray{Vec3{0, 0, 1.0}, Vec3{1, 0, 0}, 30.0};
    auto result = engine.cast(ray);
    EXPECT_TRUE(result.hit);
    EXPECT_NEAR(result.distance, 9.9, 0.15);  // до ближней грани: 10 - 0.1
}

TEST(RayCast, RayHitsSphere) {
    RaycastEngine engine;

    WorldPrimitive sphere;
    sphere.type = "sphere";
    sphere.pose.x = 5.0;
    sphere.pose.y = 0.0;
    sphere.pose.z = 0.0;
    sphere.radius = 1.0;
    engine.set_static_geometry({sphere});

    Ray ray{Vec3{0, 0, 0}, Vec3{1, 0, 0}, 30.0};
    auto result = engine.cast(ray);
    EXPECT_TRUE(result.hit);
    EXPECT_NEAR(result.distance, 4.0, 0.01);  // 5 - 1 = 4
}

TEST(RayCast, RayHitsCylinder) {
    RaycastEngine engine;

    WorldPrimitive cyl;
    cyl.type = "cylinder";
    cyl.pose.x = 7.0;
    cyl.pose.y = 0.0;
    cyl.pose.z = 1.0;
    cyl.radius = 0.5;
    cyl.height = 2.0;
    engine.set_static_geometry({cyl});

    Ray ray{Vec3{0, 0, 1.0}, Vec3{1, 0, 0}, 30.0};
    auto result = engine.cast(ray);
    EXPECT_TRUE(result.hit);
    EXPECT_NEAR(result.distance, 6.5, 0.01);  // 7 - 0.5
}

TEST(RayCast, RayTooFar) {
    RaycastEngine engine;

    WorldPrimitive wall;
    wall.type = "box";
    wall.pose.x = 50.0;
    wall.pose.y = 0.0;
    wall.pose.z = 1.0;
    wall.size = Vec3{0.2, 10.0, 2.0};
    engine.set_static_geometry({wall});

    // Стена за пределами max_range
    Ray ray{Vec3{0, 0, 1.0}, Vec3{1, 0, 0}, 30.0};
    auto result = engine.cast(ray);
    EXPECT_FALSE(result.hit);
}

TEST(RayCast, BatchCast) {
    RaycastEngine engine;

    // Комната 10x10 с 4 стенами
    std::vector<WorldPrimitive> walls;

    WorldPrimitive wall1;
    wall1.type = "box"; wall1.pose.x = 10; wall1.pose.y = 0; wall1.pose.z = 1;
    wall1.size = Vec3{20, 0.2, 2}; wall1.color = "#808080";
    walls.push_back(wall1);

    WorldPrimitive wall2;
    wall2.type = "box"; wall2.pose.x = -10; wall2.pose.y = 0; wall2.pose.z = 1;
    wall2.size = Vec3{20, 0.2, 2}; wall2.color = "#808080";
    walls.push_back(wall2);

    WorldPrimitive wall3;
    wall3.type = "box"; wall3.pose.x = 0; wall3.pose.y = 10; wall3.pose.z = 1;
    wall3.size = Vec3{0.2, 20, 2}; wall3.color = "#808080";
    walls.push_back(wall3);

    WorldPrimitive wall4;
    wall4.type = "box"; wall4.pose.x = 0; wall4.pose.y = -10; wall4.pose.z = 1;
    wall4.size = Vec3{0.2, 20, 2}; wall4.color = "#808080";
    walls.push_back(wall4);

    engine.set_static_geometry(walls);

    // 360 лучей из центра
    std::vector<Ray> rays;
    for (int i = 0; i < 360; i++) {
        double angle = i * M_PI / 180.0;
        rays.push_back({
            Vec3{0, 0, 1.0},
            Vec3{std::cos(angle), std::sin(angle), 0},
            30.0
        });
    }

    auto results = engine.cast_batch(rays);
    EXPECT_EQ(results.size(), 360);

    // Все лучи должны попасть в стену
    int hits = 0;
    for (const auto& r : results) {
        if (r.hit) hits++;
    }
    EXPECT_EQ(hits, 360);
}

TEST(RayCast, NearestHitWins) {
    RaycastEngine engine;

    // Ближняя сфера и дальняя стена
    std::vector<WorldPrimitive> prims;

    WorldPrimitive wall;
    wall.type = "box"; wall.pose.x = 20; wall.pose.y = 0; wall.pose.z = 1;
    wall.size = Vec3{0.2, 10, 2}; wall.color = "#808080";
    prims.push_back(wall);

    WorldPrimitive sphere;
    sphere.type = "sphere"; sphere.pose.x = 5; sphere.pose.y = 0; sphere.pose.z = 0;
    sphere.radius = 1.0; sphere.color = "#FF0000";
    prims.push_back(sphere);

    engine.set_static_geometry(prims);

    Ray ray{Vec3{0, 0, 0}, Vec3{1, 0, 0}, 30.0};
    auto result = engine.cast(ray);
    EXPECT_TRUE(result.hit);
    // Должен попасть в ближнюю сферу (4.0), а не стену (19.9)
    EXPECT_LT(result.distance, 10.0);
}

// ─── SimWorld Collision Tests ────────────────────────────────────

TEST(WorldCollision, SphereHitsBox) {
    SimWorld world;

    WorldPrimitive wall;
    wall.type = "box";
    wall.pose.x = 5.0;
    wall.pose.y = 0.0;
    wall.pose.z = 1.0;
    wall.size = Vec3{0.2, 10.0, 2.0};
    world.add_static_primitive(wall);

    // Сфера пересекается со стеной
    EXPECT_TRUE(world.check_sphere_collision(Vec3{5.0, 0.0, 1.0}, 1.0));

    // Сфера далеко от стены
    EXPECT_FALSE(world.check_sphere_collision(Vec3{0.0, 0.0, 0.0}, 0.5));
}

TEST(WorldCollision, SphereHitsSphere) {
    SimWorld world;

    WorldPrimitive prim;
    prim.type = "sphere";
    prim.pose.x = 5.0;
    prim.pose.y = 0.0;
    prim.pose.z = 0.0;
    prim.radius = 1.0;
    world.add_static_primitive(prim);

    // Пересекаемся
    EXPECT_TRUE(world.check_sphere_collision(Vec3{5.5, 0.0, 0.0}, 0.6));

    // Не пересекаемся
    EXPECT_FALSE(world.check_sphere_collision(Vec3{0.0, 0.0, 0.0}, 0.5));
}

} // namespace s2