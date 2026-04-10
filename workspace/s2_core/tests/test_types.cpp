/**
 * @file test_types.cpp
 * Тесты для базовых типов: Vec3 (Eigen::Vector3d), Pose3D, ZoneShape.
 *
 * Покрывает:
 *  - Базовые операции Vec3 (сложение, вычитание, скаляр, длина, нормализация)
 *  - Векторные операции (dot, cross, distance)
 *  - Краевые случаи (нулевой вектор, нормализация нуля)
 *  - ZoneShape::contains() для всех трёх типов зон
 *  - Pose3D сравнение и position()
 */

#include <s2/types.hpp>

#include <gtest/gtest.h>
#include <cmath>

namespace s2
{

// ============================================================================
// Vec3 — базовые арифметические операции
// ============================================================================

TEST(Vec3, DefaultConstructorIsZero)
{
  // Eigen::Vector3d по умолчанию не инициализирован.
  // Для нулевого вектора используем Vec3::Zero().
  Vec3 v = Vec3::Zero();
  EXPECT_DOUBLE_EQ(v.x(), 0.0);
  EXPECT_DOUBLE_EQ(v.y(), 0.0);
  EXPECT_DOUBLE_EQ(v.z(), 0.0);
}

TEST(Vec3, ConstructorWithValues)
{
  Vec3 v(1.0, 2.0, 3.0);
  EXPECT_DOUBLE_EQ(v.x(), 1.0);
  EXPECT_DOUBLE_EQ(v.y(), 2.0);
  EXPECT_DOUBLE_EQ(v.z(), 3.0);
}

TEST(Vec3, Addition)
{
  Vec3 a(1.0, 2.0, 3.0);
  Vec3 b(4.0, 5.0, 6.0);
  Vec3 c = a + b;
  EXPECT_DOUBLE_EQ(c.x(), 5.0);
  EXPECT_DOUBLE_EQ(c.y(), 7.0);
  EXPECT_DOUBLE_EQ(c.z(), 9.0);
}

TEST(Vec3, Subtraction)
{
  Vec3 a(4.0, 5.0, 6.0);
  Vec3 b(1.0, 2.0, 3.0);
  Vec3 c = a - b;
  EXPECT_DOUBLE_EQ(c.x(), 3.0);
  EXPECT_DOUBLE_EQ(c.y(), 3.0);
  EXPECT_DOUBLE_EQ(c.z(), 3.0);
}

TEST(Vec3, ScalarMultiplication)
{
  Vec3 v(1.0, 2.0, 3.0);
  Vec3 c = v * 2.0;
  EXPECT_DOUBLE_EQ(c.x(), 2.0);
  EXPECT_DOUBLE_EQ(c.y(), 4.0);
  EXPECT_DOUBLE_EQ(c.z(), 6.0);
}

TEST(Vec3, ScalarMultiplicationReversed)
{
  Vec3 v(1.0, 2.0, 3.0);
  Vec3 c = 3.0 * v;
  EXPECT_DOUBLE_EQ(c.x(), 3.0);
  EXPECT_DOUBLE_EQ(c.y(), 6.0);
  EXPECT_DOUBLE_EQ(c.z(), 9.0);
}

TEST(Vec3, Division)
{
  Vec3 v(2.0, 4.0, 6.0);
  Vec3 c = v / 2.0;
  EXPECT_DOUBLE_EQ(c.x(), 1.0);
  EXPECT_DOUBLE_EQ(c.y(), 2.0);
  EXPECT_DOUBLE_EQ(c.z(), 3.0);
}

TEST(Vec3, CompoundAddition)
{
  Vec3 v(1.0, 2.0, 3.0);
  v += Vec3(4.0, 5.0, 6.0);
  EXPECT_DOUBLE_EQ(v.x(), 5.0);
  EXPECT_DOUBLE_EQ(v.y(), 7.0);
  EXPECT_DOUBLE_EQ(v.z(), 9.0);
}

TEST(Vec3, CompoundSubtraction)
{
  Vec3 v(5.0, 7.0, 9.0);
  v -= Vec3(1.0, 2.0, 3.0);
  EXPECT_DOUBLE_EQ(v.x(), 4.0);
  EXPECT_DOUBLE_EQ(v.y(), 5.0);
  EXPECT_DOUBLE_EQ(v.z(), 6.0);
}

TEST(Vec3, CompoundScalarMultiplication)
{
  Vec3 v(1.0, 2.0, 3.0);
  v *= 3.0;
  EXPECT_DOUBLE_EQ(v.x(), 3.0);
  EXPECT_DOUBLE_EQ(v.y(), 6.0);
  EXPECT_DOUBLE_EQ(v.z(), 9.0);
}

// ============================================================================
// Vec3 — векторные операции
// ============================================================================

TEST(Vec3, DotProduct)
{
  Vec3 a(1.0, 2.0, 3.0);
  Vec3 b(4.0, 5.0, 6.0);
  // 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
  EXPECT_DOUBLE_EQ(a.dot(b), 32.0);
}

TEST(Vec3, CrossProduct)
{
  Vec3 a(1.0, 0.0, 0.0);
  Vec3 b(0.0, 1.0, 0.0);
  Vec3 c = a.cross(b);
  // i × j = k
  EXPECT_DOUBLE_EQ(c.x(), 0.0);
  EXPECT_DOUBLE_EQ(c.y(), 0.0);
  EXPECT_DOUBLE_EQ(c.z(), 1.0);
}

TEST(Vec3, Length)
{
  Vec3 v(3.0, 4.0, 0.0);
  EXPECT_DOUBLE_EQ(v.norm(), 5.0);
}

TEST(Vec3, SquaredNorm)
{
  Vec3 v(3.0, 4.0, 0.0);
  EXPECT_DOUBLE_EQ(v.squaredNorm(), 25.0);
}

TEST(Vec3, Normalized)
{
  Vec3 v(3.0, 4.0, 0.0);
  Vec3 n = v.normalized();
  EXPECT_DOUBLE_EQ(n.norm(), 1.0);
  EXPECT_NEAR(n.x(), 0.6, 1e-10);
  EXPECT_NEAR(n.y(), 0.8, 1e-10);
  EXPECT_NEAR(n.z(), 0.0, 1e-10);
}

TEST(Vec3, NormalizeZeroVector)
{
  // Eigen::Vector3d::normalized() при нулевом векторе возвращает (0, 0, 0).
  // Это поведение Eigen — прикладной код должен проверять длину перед нормализацией.
  Vec3 v(0.0, 0.0, 0.0);
  Vec3 n = v.normalized();
  EXPECT_DOUBLE_EQ(n.x(), 0.0);
  EXPECT_DOUBLE_EQ(n.y(), 0.0);
  EXPECT_DOUBLE_EQ(n.z(), 0.0);
}

TEST(Vec3, NormalizeVerySmallVector)
{
  // Eigen::Vector3d::normalized() не проверяет на ноль —
  // он нормализует любой ненулевой вектор.
  // Для защиты от деления на ноль нужна обёртка в прикладном коде.
  Vec3 v(1e-15, 1e-15, 1e-15);
  Vec3 n = v.normalized();
  // Все компоненты равны 1/sqrt(3) ≈ 0.577
  EXPECT_NEAR(n.x(), 0.57735026918962584, 1e-10);
  EXPECT_NEAR(n.y(), 0.57735026918962584, 1e-10);
  EXPECT_NEAR(n.z(), 0.57735026918962584, 1e-10);
}

TEST(Vec3, DistanceTo)
{
  Vec3 a(0.0, 0.0, 0.0);
  Vec3 b(3.0, 4.0, 0.0);
  // Eigen не имеет метода distance(), используем (a - b).norm()
  EXPECT_DOUBLE_EQ((a - b).norm(), 5.0);
}

TEST(Vec3, Equality)
{
  Vec3 a(1.0, 2.0, 3.0);
  Vec3 b(1.0, 2.0, 3.0);
  EXPECT_TRUE(a == b);
}

TEST(Vec3, Inequality)
{
  Vec3 a(1.0, 2.0, 3.0);
  Vec3 b(1.0, 2.0, 3.000001);
  EXPECT_TRUE(a != b);
}

TEST(Vec3, EqualityWithTolerance)
{
  // Eigen::operator== — точное побитовое сравнение.
  // Для сравнения с допуском используем isApprox().
  Vec3 a(1.0, 2.0, 3.0);
  Vec3 b(1.0 + 1e-10, 2.0 + 1e-10, 3.0 + 1e-10);
  EXPECT_TRUE(a.isApprox(b, 1e-9));
}

// ============================================================================
// Vec3 — операции с нулевым вектором
// ============================================================================

TEST(Vec3, ZeroPlusVector)
{
  Vec3 zero = Vec3::Zero();
  Vec3 v(1.0, 2.0, 3.0);
  EXPECT_EQ(zero + v, v);
}

TEST(Vec3, VectorMinusItself)
{
  Vec3 v(1.0, 2.0, 3.0);
  Vec3 result = v - v;
  EXPECT_EQ(result, Vec3::Zero());
}

// ============================================================================
// Pose3D
// ============================================================================

TEST(Pose3D, DefaultIsZero)
{
  Pose3D p;
  EXPECT_DOUBLE_EQ(p.x, 0.0);
  EXPECT_DOUBLE_EQ(p.y, 0.0);
  EXPECT_DOUBLE_EQ(p.z, 0.0);
  EXPECT_DOUBLE_EQ(p.roll, 0.0);
  EXPECT_DOUBLE_EQ(p.pitch, 0.0);
  EXPECT_DOUBLE_EQ(p.yaw, 0.0);
}

TEST(Pose3D, PositionReturnsVec3)
{
  Pose3D p(1.0, 2.0, 3.0, 0.0, 0.0, 0.0);
  Vec3 pos = p.position();
  EXPECT_DOUBLE_EQ(pos.x(), 1.0);
  EXPECT_DOUBLE_EQ(pos.y(), 2.0);
  EXPECT_DOUBLE_EQ(pos.z(), 3.0);
}

TEST(Pose3D, Equality)
{
  Pose3D a(1.0, 2.0, 3.0, 0.1, 0.2, 0.3);
  Pose3D b(1.0, 2.0, 3.0, 0.1, 0.2, 0.3);
  EXPECT_TRUE(a == b);
}

TEST(Pose3D, Inequality)
{
  Pose3D a(1.0, 2.0, 3.0, 0.1, 0.2, 0.3);
  Pose3D b(1.0, 2.0, 3.0, 0.1, 0.2, 0.300001);
  EXPECT_TRUE(a != b);
}

// ============================================================================
// ZoneShape::contains()
// ============================================================================

TEST(ZoneShape, SphereContainsCenter)
{
  ZoneShape shape;
  shape.type = ZoneShapeType::SPHERE;
  shape.center = Vec3(0.0, 0.0, 0.0);
  shape.radius = 5.0;

  EXPECT_TRUE(shape.contains(Vec3(0.0, 0.0, 0.0)));
}

TEST(ZoneShape, SphereContainsInside)
{
  ZoneShape shape;
  shape.type = ZoneShapeType::SPHERE;
  shape.center = Vec3(0.0, 0.0, 0.0);
  shape.radius = 5.0;

  EXPECT_TRUE(shape.contains(Vec3(3.0, 4.0, 0.0)));  // distance = 5.0
}

TEST(ZoneShape, SphereContainsBoundary)
{
  // Точка на границе сферы должна считаться внутри (<=)
  ZoneShape shape;
  shape.type = ZoneShapeType::SPHERE;
  shape.center = Vec3(0.0, 0.0, 0.0);
  shape.radius = 5.0;

  EXPECT_TRUE(shape.contains(Vec3(5.0, 0.0, 0.0)));
}

TEST(ZoneShape, SphereDoesNotContainOutside)
{
  ZoneShape shape;
  shape.type = ZoneShapeType::SPHERE;
  shape.center = Vec3(0.0, 0.0, 0.0);
  shape.radius = 5.0;

  EXPECT_FALSE(shape.contains(Vec3(6.0, 0.0, 0.0)));
}

TEST(ZoneShape, SphereWithOffsetCenter)
{
  ZoneShape shape;
  shape.type = ZoneShapeType::SPHERE;
  shape.center = Vec3(10.0, 10.0, 10.0);
  shape.radius = 1.0;

  EXPECT_TRUE(shape.contains(Vec3(10.5, 10.5, 10.0)));
  EXPECT_FALSE(shape.contains(Vec3(0.0, 0.0, 0.0)));
}

TEST(ZoneShape, AabbContainsCenter)
{
  ZoneShape shape;
  shape.type = ZoneShapeType::AABB;
  shape.center = Vec3(0.0, 0.0, 0.0);
  shape.half_size = Vec3(1.0, 1.0, 1.0);

  EXPECT_TRUE(shape.contains(Vec3(0.0, 0.0, 0.0)));
}

TEST(ZoneShape, AabbContainsInside)
{
  ZoneShape shape;
  shape.type = ZoneShapeType::AABB;
  shape.center = Vec3(0.0, 0.0, 0.0);
  shape.half_size = Vec3(2.0, 3.0, 4.0);

  EXPECT_TRUE(shape.contains(Vec3(1.0, 2.0, 3.0)));
}

TEST(ZoneShape, AabbContainsBoundary)
{
  // Точка на грани AABB должна считаться внутри (<=)
  ZoneShape shape;
  shape.type = ZoneShapeType::AABB;
  shape.center = Vec3(0.0, 0.0, 0.0);
  shape.half_size = Vec3(1.0, 1.0, 1.0);

  EXPECT_TRUE(shape.contains(Vec3(1.0, 0.0, 0.0)));
  EXPECT_TRUE(shape.contains(Vec3(0.0, 1.0, 0.0)));
  EXPECT_TRUE(shape.contains(Vec3(0.0, 0.0, 1.0)));
  EXPECT_TRUE(shape.contains(Vec3(1.0, 1.0, 1.0)));
}

TEST(ZoneShape, AabbDoesNotContainOutside)
{
  ZoneShape shape;
  shape.type = ZoneShapeType::AABB;
  shape.center = Vec3(0.0, 0.0, 0.0);
  shape.half_size = Vec3(1.0, 1.0, 1.0);

  EXPECT_FALSE(shape.contains(Vec3(1.1, 0.0, 0.0)));
}

TEST(ZoneShape, AabbWithOffsetCenter)
{
  ZoneShape shape;
  shape.type = ZoneShapeType::AABB;
  shape.center = Vec3(10.0, 20.0, 30.0);
  shape.half_size = Vec3(5.0, 5.0, 5.0);

  EXPECT_TRUE(shape.contains(Vec3(12.0, 22.0, 32.0)));
  EXPECT_FALSE(shape.contains(Vec3(0.0, 0.0, 0.0)));
}

TEST(ZoneShape, AabbWithNegativeCoordinates)
{
  ZoneShape shape;
  shape.type = ZoneShapeType::AABB;
  shape.center = Vec3(-5.0, -5.0, -5.0);
  shape.half_size = Vec3(2.0, 2.0, 2.0);

  EXPECT_TRUE(shape.contains(Vec3(-5.0, -5.0, -5.0)));
  EXPECT_TRUE(shape.contains(Vec3(-6.0, -6.0, -6.0)));
  EXPECT_FALSE(shape.contains(Vec3(0.0, 0.0, 0.0)));
}

TEST(ZoneShape, InfiniteContainsEverything)
{
  ZoneShape shape;
  shape.type = ZoneShapeType::INFINITE;

  EXPECT_TRUE(shape.contains(Vec3(0.0, 0.0, 0.0)));
  EXPECT_TRUE(shape.contains(Vec3(1e15, 1e15, 1e15)));
  EXPECT_TRUE(shape.contains(Vec3(-1e15, -1e15, -1e15)));
}

// ============================================================================
// Velocity
// ============================================================================

TEST(Velocity, DefaultIsZero)
{
  Velocity v;
  EXPECT_EQ(v.linear, Vec3::Zero());
  EXPECT_EQ(v.angular, Vec3::Zero());
}

// ============================================================================
// DesiredVelocity
// ============================================================================

TEST(DesiredVelocity, DefaultIsValid)
{
  DesiredVelocity v;
  EXPECT_EQ(v.linear, Vec3::Zero());
  EXPECT_EQ(v.angular, Vec3::Zero());
  EXPECT_TRUE(v.valid);
}

// ============================================================================
// Transform3D
// ============================================================================

TEST(Transform3D, DefaultIsIdentity)
{
  Transform3D t;
  EXPECT_EQ(t.translation, Vec3::Zero());
  EXPECT_EQ(t.rotation, Eigen::Matrix3d::Identity());
}

TEST(Transform3D, TransformPoint)
{
  Transform3D t;
  t.translation = Vec3(10.0, 0.0, 0.0);
  t.rotation = Eigen::Matrix3d::Identity();

  Vec3 local(1.0, 0.0, 0.0);
  Vec3 world = t.transform_point(local);

  EXPECT_NEAR(world.x(), 11.0, 1e-10);
  EXPECT_NEAR(world.y(), 0.0, 1e-10);
  EXPECT_NEAR(world.z(), 0.0, 1e-10);
}

TEST(Transform3D, InverseTransformPoint)
{
  Transform3D t;
  t.translation = Vec3(10.0, 0.0, 0.0);
  t.rotation = Eigen::Matrix3d::Identity();

  Vec3 world(11.0, 0.0, 0.0);
  Vec3 local = t.inverse_transform_point(world);

  EXPECT_NEAR(local.x(), 1.0, 1e-10);
  EXPECT_NEAR(local.y(), 0.0, 1e-10);
  EXPECT_NEAR(local.z(), 0.0, 1e-10);
}

}  // namespace s2