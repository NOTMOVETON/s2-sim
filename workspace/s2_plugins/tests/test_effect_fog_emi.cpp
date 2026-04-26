#include <gtest/gtest.h>
#include <s2/effects/fog_effect.hpp>
#include <s2/effects/emi_effect.hpp>
#include <s2/effect_context.hpp>

// ── FogEffect ────────────────────────────────────────────────────────────────

TEST(FogEffect, EffectTypeSensor) {
    s2::FogEffect fog;
    YAML::Node params;
    fog.on_init(params);
    EXPECT_EQ(fog.effect_type(), s2::EffectType::SENSOR);
}

TEST(FogEffect, RequiresOpticalSensor) {
    s2::FogEffect fog;
    YAML::Node params;
    fog.on_init(params);
    auto caps = fog.required_capabilities();
    ASSERT_EQ(caps.size(), 1u);
    EXPECT_EQ(caps[0], "optical_sensor");
}

TEST(FogEffect, SensorModFullStrength) {
    s2::FogEffect fog;
    YAML::Node params;
    params["range_multiplier"] = 0.3;
    fog.on_init(params);
    s2::EffectContext ctx;
    ctx.zone_strength = 1.0;
    auto mods = fog.sensor_mods(ctx);
    ASSERT_EQ(mods.size(), 1u);
    EXPECT_EQ(mods[0].param, "max_range");
    EXPECT_NEAR(mods[0].multiplier, 0.3, 1e-9);
}

TEST(FogEffect, SensorModHalfStrength) {
    s2::FogEffect fog;
    YAML::Node params;
    params["range_multiplier"] = 0.3;
    fog.on_init(params);
    s2::EffectContext ctx;
    ctx.zone_strength = 0.5;
    auto mods = fog.sensor_mods(ctx);
    ASSERT_EQ(mods.size(), 1u);
    // 0.3 + (1.0 - 0.3) * (1.0 - 0.5) = 0.65
    EXPECT_NEAR(mods[0].multiplier, 0.65, 1e-9);
}

TEST(FogEffect, VisualHintGlow) {
    s2::FogEffect fog;
    YAML::Node params;
    fog.on_init(params);
    auto hint = fog.visual_hint();
    ASSERT_TRUE(hint.has_value());
    EXPECT_EQ(hint->type, "glow");
}

// ── EMIEffect ────────────────────────────────────────────────────────────────

TEST(EMIEffect, EffectTypeSensor) {
    s2::EMIEffect emi;
    YAML::Node params;
    emi.on_init(params);
    EXPECT_EQ(emi.effect_type(), s2::EffectType::SENSOR);
}

TEST(EMIEffect, RequiresGnssAndImu) {
    s2::EMIEffect emi;
    YAML::Node params;
    emi.on_init(params);
    auto caps = emi.required_capabilities();
    ASSERT_EQ(caps.size(), 2u);
    EXPECT_EQ(caps[0], "gnss_sensor");
    EXPECT_EQ(caps[1], "imu_sensor");
}

TEST(EMIEffect, SensorModFullStrength) {
    s2::EMIEffect emi;
    YAML::Node params;
    params["noise_addend"] = 0.5;
    emi.on_init(params);
    s2::EffectContext ctx;
    ctx.zone_strength = 1.0;
    auto mods = emi.sensor_mods(ctx);
    ASSERT_EQ(mods.size(), 1u);
    EXPECT_EQ(mods[0].param, "noise_std");
    EXPECT_NEAR(mods[0].addend, 0.5, 1e-9);
}

TEST(EMIEffect, SensorModHalfStrength) {
    s2::EMIEffect emi;
    YAML::Node params;
    params["noise_addend"] = 0.5;
    emi.on_init(params);
    s2::EffectContext ctx;
    ctx.zone_strength = 0.5;
    auto mods = emi.sensor_mods(ctx);
    ASSERT_EQ(mods.size(), 1u);
    EXPECT_NEAR(mods[0].addend, 0.25, 1e-9);
}

TEST(EMIEffect, VisualHintGlowOrange) {
    s2::EMIEffect emi;
    YAML::Node params;
    emi.on_init(params);
    auto hint = emi.visual_hint();
    ASSERT_TRUE(hint.has_value());
    EXPECT_EQ(hint->type, "glow");
    EXPECT_EQ(hint->params["color"].get<std::string>(), "#FFAA22");
}
