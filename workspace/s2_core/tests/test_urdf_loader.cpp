/**
 * @file test_urdf_loader.cpp
 * Тесты загрузчика кинематического дерева из URDF.
 */

#include <s2/urdf_loader.hpp>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

namespace
{

// Путь к тестовому URDF — задаётся через CMake compile definition
static std::string urdf_path()
{
#ifdef S2_TEST_URDF_PATH
    return S2_TEST_URDF_PATH;
#else
    // Fallback: ищем рядом с исходником
    std::filesystem::path candidate =
        std::filesystem::path(__FILE__).parent_path() / "test_urdf.xml";
    if (std::filesystem::exists(candidate))
        return candidate.string();
    return (std::filesystem::current_path() / "test_urdf.xml").string();
#endif
}

} // anonymous namespace

// ─── load_urdf: базовая загрузка ──────────────────────────────────────────

TEST(UrdfLoaderTest, LoadsLinksFromFile)
{
    s2::KinematicTree tree = s2::load_urdf(urdf_path(), "base_link");
    EXPECT_FALSE(tree.empty());
}

TEST(UrdfLoaderTest, ContainsRootLink)
{
    s2::KinematicTree tree = s2::load_urdf(urdf_path(), "base_link");

    bool found_root = false;
    for (const auto& link : tree.links()) {
        if (link.name == "base_link" && link.parent.empty()) {
            found_root = true;
            break;
        }
    }
    EXPECT_TRUE(found_root) << "Корневое звено base_link не найдено";
}

TEST(UrdfLoaderTest, ContainsAllExpectedLinks)
{
    s2::KinematicTree tree = s2::load_urdf(urdf_path(), "base_link");

    std::set<std::string> names;
    for (const auto& link : tree.links()) {
        names.insert(link.name);
    }

    EXPECT_TRUE(names.count("base_link"));
    EXPECT_TRUE(names.count("gps_link"));
    EXPECT_TRUE(names.count("arm"));
    EXPECT_TRUE(names.count("FL_wheel"));
    EXPECT_TRUE(names.count("bucket"));
}

TEST(UrdfLoaderTest, JointTypesAreCorrect)
{
    s2::KinematicTree tree = s2::load_urdf(urdf_path(), "base_link");

    for (const auto& link : tree.links()) {
        if (link.name == "gps_link") {
            EXPECT_EQ(link.joint.type, s2::JointType::FIXED);
        } else if (link.name == "arm") {
            EXPECT_EQ(link.joint.type, s2::JointType::REVOLUTE);
        } else if (link.name == "FL_wheel") {
            EXPECT_EQ(link.joint.type, s2::JointType::CONTINUOUS);
        } else if (link.name == "bucket") {
            EXPECT_EQ(link.joint.type, s2::JointType::PRISMATIC);
        }
    }
}

TEST(UrdfLoaderTest, OriginParsedCorrectly)
{
    s2::KinematicTree tree = s2::load_urdf(urdf_path(), "base_link");

    for (const auto& link : tree.links()) {
        if (link.name == "gps_link") {
            EXPECT_NEAR(link.origin.x, 0.1, 1e-6);
            EXPECT_NEAR(link.origin.y, 0.0, 1e-6);
            EXPECT_NEAR(link.origin.z, 0.3, 1e-6);
        } else if (link.name == "arm") {
            EXPECT_NEAR(link.origin.x, 0.5, 1e-6);
            EXPECT_NEAR(link.origin.z, 0.2, 1e-6);
        }
    }
}

TEST(UrdfLoaderTest, LimitsAreCorrect)
{
    s2::KinematicTree tree = s2::load_urdf(urdf_path(), "base_link");

    for (const auto& link : tree.links()) {
        if (link.name == "arm") {
            EXPECT_NEAR(link.joint.min, -1.57, 1e-2);
            EXPECT_NEAR(link.joint.max,  1.57, 1e-2);
        } else if (link.name == "bucket") {
            EXPECT_NEAR(link.joint.min, 0.0, 1e-6);
            EXPECT_NEAR(link.joint.max, 0.5, 1e-6);
        }
    }
}

TEST(UrdfLoaderTest, ParentLinksAreCorrect)
{
    s2::KinematicTree tree = s2::load_urdf(urdf_path(), "base_link");

    for (const auto& link : tree.links()) {
        if (link.name == "base_link") {
            EXPECT_TRUE(link.parent.empty());
        } else if (link.name == "gps_link" || link.name == "arm" || link.name == "FL_wheel") {
            EXPECT_EQ(link.parent, "base_link");
        } else if (link.name == "bucket") {
            EXPECT_EQ(link.parent, "arm");
        }
    }
}

TEST(UrdfLoaderTest, ThrowsOnMissingFile)
{
    EXPECT_THROW(s2::load_urdf("/nonexistent/path/robot.urdf"), std::runtime_error);
}

TEST(UrdfLoaderTest, AxisParsedCorrectly)
{
    s2::KinematicTree tree = s2::load_urdf(urdf_path(), "base_link");

    for (const auto& link : tree.links()) {
        if (link.name == "arm") {
            EXPECT_NEAR(link.joint.axis.x(), 0.0, 1e-6);
            EXPECT_NEAR(link.joint.axis.y(), 1.0, 1e-6);
            EXPECT_NEAR(link.joint.axis.z(), 0.0, 1e-6);
        }
    }
}
