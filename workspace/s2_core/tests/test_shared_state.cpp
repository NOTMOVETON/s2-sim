/**
 * @file test_shared_state.cpp
 * Тесты для SharedState: single-owner storage, contributions, resolver.
 *
 * Покрывает:
 *  - emplace/get/has для single-owner state
 *  - Contributions: scale, lock, velocity_addition
 *  - Resolver: product для scale, OR для locks, sum для additive
 *  - Краевые случаи: пустые contributions, отрицательные значения, clamp
 *  - clear_contributions() сбрасывает effective
 *  - Источники сохраняются для отладки
 */

#include <s2/shared_state.hpp>

#include <gtest/gtest.h>
#include <cmath>

namespace s2
{

// Вспомогательный компонент для тестирования single-owner state
struct BatteryComponent
{
  double level{100.0};
  BatteryComponent() = default;
  explicit BatteryComponent(double lvl) : level(lvl) {}
};

// Вспомогательный компонент для тестирования held objects
struct HeldObjects
{
  std::vector<ObjectId> objects;
};

// ============================================================================
// Single-owner state: emplace / get / has
// ============================================================================

TEST(SharedState, EmplaceAndGet)
{
  SharedState state;

  // Размещаем компонент
  auto& battery = state.emplace<BatteryComponent>(75.0);
  EXPECT_DOUBLE_EQ(battery.level, 75.0);

  // Получаем через get()
  auto* retrieved = state.get<BatteryComponent>();
  ASSERT_NE(retrieved, nullptr);
  EXPECT_DOUBLE_EQ(retrieved->level, 75.0);

  // Const get()
  const auto* const_retrieved = state.get<BatteryComponent>();
  ASSERT_NE(const_retrieved, nullptr);
  EXPECT_DOUBLE_EQ(const_retrieved->level, 75.0);
}

TEST(SharedState, HasReturnsTrue)
{
  SharedState state;
  state.emplace<BatteryComponent>(100.0);
  EXPECT_TRUE(state.has<BatteryComponent>());
}

TEST(SharedState, HasReturnsFalseForMissingType)
{
  SharedState state;
  EXPECT_FALSE(state.has<BatteryComponent>());
}

TEST(SharedState, GetReturnsNullptrForMissingType)
{
  SharedState state;
  EXPECT_EQ(state.get<BatteryComponent>(), nullptr);
}

TEST(SharedState, EmplaceOverwritesExisting)
{
  SharedState state;
  state.emplace<BatteryComponent>(50.0);
  state.emplace<BatteryComponent>(90.0);

  auto* battery = state.get<BatteryComponent>();
  ASSERT_NE(battery, nullptr);
  EXPECT_DOUBLE_EQ(battery->level, 90.0);
}

TEST(SharedState, MultipleTypesCoexist)
{
  SharedState state;
  state.emplace<BatteryComponent>(80.0);
  state.emplace<HeldObjects>();

  auto* battery = state.get<BatteryComponent>();
  auto* objects = state.get<HeldObjects>();

  ASSERT_NE(battery, nullptr);
  ASSERT_NE(objects, nullptr);
  EXPECT_DOUBLE_EQ(battery->level, 80.0);
  EXPECT_TRUE(objects->objects.empty());
}

TEST(SharedState, ModifyThroughGet)
{
  SharedState state;
  state.emplace<BatteryComponent>(100.0);

  auto* battery = state.get<BatteryComponent>();
  ASSERT_NE(battery, nullptr);
  battery->level = 50.0;

  // Проверяем, что изменение сохранилось
  EXPECT_DOUBLE_EQ(state.get<BatteryComponent>()->level, 50.0);
}

// ============================================================================
// Scale contributions: product
// ============================================================================

TEST(SharedState, ScaleProduct)
{
  SharedState state;
  state.add_scale(0.8, "battery");
  state.add_scale(0.5, "ice_zone");
  state.add_scale(1.2, "boost_zone");

  state.resolve();

  // 0.8 × 0.5 × 1.2 = 0.48
  EXPECT_NEAR(state.effective().speed_scale, 0.48, 1e-10);
}

TEST(SharedState, ScaleNoContributions)
{
  // Пустой список = 1.0 (без ограничений)
  SharedState state;
  state.resolve();
  EXPECT_DOUBLE_EQ(state.effective().speed_scale, 1.0);
}

TEST(SharedState, ScaleSingleContribution)
{
  SharedState state;
  state.add_scale(0.7, "battery");
  state.resolve();
  EXPECT_DOUBLE_EQ(state.effective().speed_scale, 0.7);
}

TEST(SharedState, ScaleContributionEqualsOne)
{
  // Contribution = 1.0 → результат = 1.0
  SharedState state;
  state.add_scale(1.0, "normal");
  state.resolve();
  EXPECT_DOUBLE_EQ(state.effective().speed_scale, 1.0);
}

TEST(SharedState, ScaleContributionEqualsZero)
{
  // Contribution = 0.0 → результат = 0.0
  SharedState state;
  state.add_scale(0.0, "full_stop");
  state.resolve();
  EXPECT_DOUBLE_EQ(state.effective().speed_scale, 0.0);
}

TEST(SharedState, ScaleClampToZero)
{
  // Отрицательный scale → clamp до 0.0
  SharedState state;
  state.add_scale(-0.5, "negative");
  state.resolve();
  EXPECT_DOUBLE_EQ(state.effective().speed_scale, 0.0);
}

TEST(SharedState, ScaleClampToMax)
{
  // Очень большое значение → clamp до 10.0
  SharedState state;
  state.add_scale(20.0, "super_boost");
  state.resolve();
  EXPECT_DOUBLE_EQ(state.effective().speed_scale, 10.0);
}

TEST(SharedState, ScaleMultipleContributionsProduct)
{
  // 0.85 × 0.20 × 1.15 × 0.80 = 0.1564
  SharedState state;
  state.add_scale(0.85, "battery");
  state.add_scale(0.20, "ice_zone");
  state.add_scale(1.15, "boost_zone");
  state.add_scale(0.80, "payload");
  state.resolve();

  EXPECT_NEAR(state.effective().speed_scale, 0.1564, 1e-10);
}

// ============================================================================
// Lock contributions: OR
// ============================================================================

TEST(SharedState, LockNoContributions)
{
  // Пустой список = false (движение разрешено)
  SharedState state;
  state.resolve();
  EXPECT_FALSE(state.effective().motion_locked);
}

TEST(SharedState, LockSingleTrue)
{
  SharedState state;
  state.add_lock(true, "estop");
  state.resolve();
  EXPECT_TRUE(state.effective().motion_locked);
}

TEST(SharedState, LockSingleFalse)
{
  SharedState state;
  state.add_lock(false, "normal");
  state.resolve();
  EXPECT_FALSE(state.effective().motion_locked);
}

TEST(SharedState, LockMultipleAllFalse)
{
  SharedState state;
  state.add_lock(false, "source_a");
  state.add_lock(false, "source_b");
  state.resolve();
  EXPECT_FALSE(state.effective().motion_locked);
}

TEST(SharedState, LockMultipleOneTrue)
{
  // Один true среди false → locked
  SharedState state;
  state.add_lock(false, "source_a");
  state.add_lock(true, "estop");
  state.add_lock(false, "source_b");
  state.resolve();
  EXPECT_TRUE(state.effective().motion_locked);
}

// ============================================================================
// Additive contributions: sum
// ============================================================================

TEST(SharedState, AdditiveNoContributions)
{
  // Пустой список = Vec3::Zero()
  SharedState state;
  state.resolve();
  EXPECT_EQ(state.effective().velocity_addition, Vec3::Zero());
}

TEST(SharedState, AdditiveSingle)
{
  SharedState state;
  state.add_velocity_addition(Vec3(1.0, 0.0, 0.0), "conveyor");
  state.resolve();

  EXPECT_NEAR(state.effective().velocity_addition.x(), 1.0, 1e-10);
  EXPECT_NEAR(state.effective().velocity_addition.y(), 0.0, 1e-10);
  EXPECT_NEAR(state.effective().velocity_addition.z(), 0.0, 1e-10);
}

TEST(SharedState, AdditiveMultiple)
{
  // (1, 0, 0) + (0, 1, 0) = (1, 1, 0)
  SharedState state;
  state.add_velocity_addition(Vec3(1.0, 0.0, 0.0), "conveyor");
  state.add_velocity_addition(Vec3(0.0, 1.0, 0.0), "wind");
  state.resolve();

  EXPECT_NEAR(state.effective().velocity_addition.x(), 1.0, 1e-10);
  EXPECT_NEAR(state.effective().velocity_addition.y(), 1.0, 1e-10);
  EXPECT_NEAR(state.effective().velocity_addition.z(), 0.0, 1e-10);
}

// ============================================================================
// clear_contributions()
// ============================================================================

TEST(SharedState, ClearContributionsResetsEffective)
{
  SharedState state;
  state.add_scale(0.5, "battery");
  state.add_lock(true, "estop");
  state.add_velocity_addition(Vec3(1.0, 0.0, 0.0), "conveyor");
  state.resolve();

  // Проверяем, что contributions установлены
  EXPECT_NEAR(state.effective().speed_scale, 0.5, 1e-10);
  EXPECT_TRUE(state.effective().motion_locked);
  EXPECT_NEAR(state.effective().velocity_addition.x(), 1.0, 1e-10);

  // Очищаем
  state.clear_contributions();

  // Effective сброшены к дефолтам
  EXPECT_DOUBLE_EQ(state.effective().speed_scale, 1.0);
  EXPECT_FALSE(state.effective().motion_locked);
  EXPECT_EQ(state.effective().velocity_addition, Vec3::Zero());
}

TEST(SharedState, ClearContributionsPreservesSingleOwner)
{
  SharedState state;
  state.emplace<BatteryComponent>(50.0);
  state.add_scale(0.5, "battery");
  state.resolve();

  state.clear_contributions();

  // Single-owner state сохранился
  auto* battery = state.get<BatteryComponent>();
  ASSERT_NE(battery, nullptr);
  EXPECT_DOUBLE_EQ(battery->level, 50.0);
}

TEST(SharedState, ResolveAfterClearReturnsDefaults)
{
  SharedState state;
  state.add_scale(0.5, "battery");
  state.resolve();
  state.clear_contributions();
  state.resolve();

  // После clear + resolve — дефолтные значения
  EXPECT_DOUBLE_EQ(state.effective().speed_scale, 1.0);
  EXPECT_FALSE(state.effective().motion_locked);
  EXPECT_EQ(state.effective().velocity_addition, Vec3::Zero());
}

// ============================================================================
// Источники сохраняются для отладки
// ============================================================================

TEST(SharedState, EffectivePreservesScaleSources)
{
  SharedState state;
  state.add_scale(0.8, "battery");
  state.add_scale(0.5, "ice_zone");
  state.resolve();

  const auto& eff = state.effective();
  ASSERT_EQ(eff.scale_sources.size(), 2u);
  EXPECT_EQ(eff.scale_sources[0].source, "battery");
  EXPECT_NEAR(eff.scale_sources[0].value, 0.8, 1e-10);
  EXPECT_EQ(eff.scale_sources[1].source, "ice_zone");
  EXPECT_NEAR(eff.scale_sources[1].value, 0.5, 1e-10);
}

TEST(SharedState, EffectivePreservesLockSources)
{
  SharedState state;
  state.add_lock(true, "estop");
  state.add_lock(false, "warning");
  state.resolve();

  const auto& eff = state.effective();
  ASSERT_EQ(eff.lock_sources.size(), 2u);
  EXPECT_EQ(eff.lock_sources[0].source, "estop");
  EXPECT_TRUE(eff.lock_sources[0].locked);
  EXPECT_EQ(eff.lock_sources[1].source, "warning");
  EXPECT_FALSE(eff.lock_sources[1].locked);
}

TEST(SharedState, EffectivePreservesAdditiveSources)
{
  SharedState state;
  state.add_velocity_addition(Vec3(1.0, 0.0, 0.0), "conveyor");
  state.add_velocity_addition(Vec3(0.0, 0.5, 0.0), "wind");
  state.resolve();

  const auto& eff = state.effective();
  ASSERT_EQ(eff.additive_sources.size(), 2u);
  EXPECT_EQ(eff.additive_sources[0].source, "conveyor");
  EXPECT_EQ(eff.additive_sources[1].source, "wind");
}

// ============================================================================
// Счётчики contributions
// ============================================================================

TEST(SharedState, ContribCounts)
{
  SharedState state;
  EXPECT_EQ(state.scale_contrib_count(), 0u);
  EXPECT_EQ(state.lock_contrib_count(), 0u);
  EXPECT_EQ(state.additive_contrib_count(), 0u);

  state.add_scale(0.8, "battery");
  state.add_scale(0.5, "ice");
  state.add_lock(true, "estop");
  state.add_velocity_addition(Vec3(1.0, 0.0, 0.0), "conveyor");

  EXPECT_EQ(state.scale_contrib_count(), 2u);
  EXPECT_EQ(state.lock_contrib_count(), 1u);
  EXPECT_EQ(state.additive_contrib_count(), 1u);
}

// ============================================================================
// Интеграционный тест: полный цикл тика
// ============================================================================

TEST(SharedState, FullTickCycle)
{
  SharedState state;

  // Тик 1: BatteryResource публикует contribution
  state.emplace<BatteryComponent>(30.0);
  state.add_scale(0.6, "battery");
  state.add_lock(false, "battery");
  state.resolve();

  EXPECT_NEAR(state.effective().speed_scale, 0.6, 1e-10);
  EXPECT_FALSE(state.effective().motion_locked);

  // Тик 2: Очищаем, добавляем новые contributions
  state.clear_contributions();
  state.add_scale(0.9, "battery");  // Батарея разрядилась меньше
  state.add_scale(0.3, "ice_zone"); // Робот въехал в лёд
  state.add_lock(true, "estop");    // Кто-то нажал e-stop
  state.resolve();

  // 0.9 × 0.3 = 0.27, motion_locked = true
  EXPECT_NEAR(state.effective().speed_scale, 0.27, 1e-10);
  EXPECT_TRUE(state.effective().motion_locked);

  // Single-owner state сохранился между тиками
  auto* battery = state.get<BatteryComponent>();
  ASSERT_NE(battery, nullptr);
  EXPECT_DOUBLE_EQ(battery->level, 30.0);
}

}  // namespace s2