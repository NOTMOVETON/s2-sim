#include <gtest/gtest.h>
#include <s2/zone.hpp>

TEST(ZoneDetectionMode, DefaultIsCenter)
{
    s2::Zone z;
    EXPECT_EQ(z.detection_mode_enum, s2::DetectionMode::CENTER);
}

TEST(ZoneDetectionMode, CanSetAllModes)
{
    s2::Zone z;
    z.detection_mode_enum = s2::DetectionMode::BOUNDING;
    EXPECT_EQ(z.detection_mode_enum, s2::DetectionMode::BOUNDING);
    z.detection_mode_enum = s2::DetectionMode::PER_LINK;
    EXPECT_EQ(z.detection_mode_enum, s2::DetectionMode::PER_LINK);
}
