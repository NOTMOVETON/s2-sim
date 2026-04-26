#include <gtest/gtest.h>
#include <s2/zone.hpp>
#include <s2/effect_context.hpp>

// Тест: поля lifecycle доступны и имеют правильные дефолты
TEST(ZoneLifecycle, DefaultValues)
{
    s2::Zone z;
    EXPECT_DOUBLE_EQ(z.strength, 1.0);
    EXPECT_DOUBLE_EQ(z.lifecycle.initial_strength, 1.0);
    EXPECT_DOUBLE_EQ(z.lifecycle.growth_rate, 0.0);
    EXPECT_DOUBLE_EQ(z.lifecycle.decay_rate, 0.0);
    EXPECT_DOUBLE_EQ(z.lifecycle.decay_delay, 0.0);
    EXPECT_DOUBLE_EQ(z.lifecycle.max_strength, 1.0);
    EXPECT_DOUBLE_EQ(z.lifecycle.remove_threshold, 0.05);
}

TEST(ZoneLifecycle, CanSetStrength)
{
    s2::Zone z;
    z.strength = 0.5;
    EXPECT_DOUBLE_EQ(z.strength, 0.5);
}

TEST(ZoneLifecycle, EffectContextStrength)
{
    s2::EffectContext ctx;
    EXPECT_DOUBLE_EQ(ctx.zone_strength, 1.0);
    EXPECT_EQ(ctx.contact_link, "");
    ctx.zone_strength = 0.3;
    ctx.contact_link = "left_wheel";
    EXPECT_DOUBLE_EQ(ctx.zone_strength, 0.3);
    EXPECT_EQ(ctx.contact_link, "left_wheel");
}
