#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>

TEST(Smoke, CompilerWorks) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(Smoke, EigenAvailable) {
    Eigen::Vector3d v(1.0, 2.0, 3.0);
    EXPECT_DOUBLE_EQ(v.norm(), std::sqrt(14.0));
}