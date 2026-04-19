#include <gtest/gtest.h>

#include <s2/scene_writer.hpp>
#include <s2/scene_loader.hpp>
#include <s2/world.hpp>

#include <fstream>
#include <sstream>
#include <cstdio>
#include <filesystem>

namespace s2 {

namespace {

// Вспомогательная функция: создать временный YAML-файл сцены с геометрией
std::string make_temp_yaml(const std::string& content) {
    std::string path = std::filesystem::temp_directory_path() / "s2_test_scene_writer.yaml";
    std::ofstream f(path);
    f << content;
    return path;
}

// Загрузить geometry из YAML-файла через SceneLoader
std::vector<WorldPrimitive> load_geometry_from_yaml(const std::string& path) {
    auto data = SceneLoader::load(path);
    return data.geometry;
}

} // anonymous namespace

// ─── SceneWriter Tests ────────────────────────────────────────────

TEST(SceneWriter, SaveAndReloadGeometry) {
    // Исходная сцена с одним box
    const std::string yaml_content = R"(
s2:
  update_rate: 100
  visualizer:
    enabled: false
  world:
    surface: "flat"
    geometry:
      - type: "box"
        pose: {x: 1.0, y: 2.0, z: 0.5, yaw: 0.0}
        size: {x: 2.0, y: 1.0, z: 1.5}
        color: "#FF0000"
  agents: []
)";
    auto path = make_temp_yaml(yaml_content);

    // Новая геометрия: sphere + cylinder
    std::vector<WorldPrimitive> new_prims;
    {
        WorldPrimitive sp;
        sp.type = "sphere";
        sp.pose.x = 3.0; sp.pose.y = 4.0; sp.pose.z = 1.0;
        sp.radius = 0.75;
        sp.color = "#00FF00";
        new_prims.push_back(sp);
    }
    {
        WorldPrimitive cyl;
        cyl.type = "cylinder";
        cyl.pose.x = -1.0; cyl.pose.y = 0.5; cyl.pose.z = 2.0;
        cyl.pose.yaw = 1.57;
        cyl.radius = 0.4;
        cyl.height = 3.0;
        cyl.color = "#0000FF";
        new_prims.push_back(cyl);
    }

    ASSERT_NO_THROW(SceneWriter::save_geometry(path, new_prims));

    // Перечитываем и проверяем
    auto reloaded = load_geometry_from_yaml(path);
    ASSERT_EQ(reloaded.size(), 2u);

    EXPECT_EQ(reloaded[0].type, "sphere");
    EXPECT_NEAR(reloaded[0].pose.x, 3.0, 1e-6);
    EXPECT_NEAR(reloaded[0].pose.y, 4.0, 1e-6);
    EXPECT_NEAR(reloaded[0].pose.z, 1.0, 1e-6);
    EXPECT_NEAR(reloaded[0].radius, 0.75, 1e-6);
    EXPECT_EQ(reloaded[0].color, "#00FF00");

    EXPECT_EQ(reloaded[1].type, "cylinder");
    EXPECT_NEAR(reloaded[1].pose.x, -1.0, 1e-6);
    EXPECT_NEAR(reloaded[1].pose.yaw, 1.57, 1e-4);
    EXPECT_NEAR(reloaded[1].radius, 0.4, 1e-6);
    EXPECT_NEAR(reloaded[1].height, 3.0, 1e-6);
    EXPECT_EQ(reloaded[1].color, "#0000FF");

    std::remove(path.c_str());
}

TEST(SceneWriter, OtherSectionsPreserved) {
    // Сцена с агентом — проверяем что агент не потерялся после сохранения геометрии
    const std::string yaml_content = R"(
s2:
  update_rate: 50
  visualizer:
    enabled: false
    port: 9090
  world:
    surface: "flat"
    geometry:
      - type: "box"
        pose: {x: 0, y: 0, z: 0.5, yaw: 0}
        size: {x: 1, y: 1, z: 1}
        color: "#808080"
  agents:
    - name: "robot_0"
      pose: {x: 1.0, y: 2.0, z: 0.0, yaw: 0.785}
      collision:
        bounding: {type: "sphere", radius: 0.4}
      visual: {type: "box", size: [0.8, 0.5, 0.3], color: "#FF6B35"}
)";
    auto path = make_temp_yaml(yaml_content);

    // Заменяем геометрию
    std::vector<WorldPrimitive> new_prims;
    WorldPrimitive box;
    box.type = "box";
    box.pose.x = 5.0; box.pose.z = 1.0;
    box.size = Vec3{3.0, 2.0, 2.0};
    box.color = "#AAAAAA";
    new_prims.push_back(box);

    ASSERT_NO_THROW(SceneWriter::save_geometry(path, new_prims));

    // Проверяем что агент сохранился
    auto data = SceneLoader::load(path);
    ASSERT_EQ(data.agents.size(), 1u);
    EXPECT_EQ(data.agents[0].name, "robot_0");
    EXPECT_NEAR(data.agents[0].world_pose.x, 1.0, 1e-6);
    EXPECT_NEAR(data.agents[0].world_pose.yaw, 0.785, 1e-4);

    // Проверяем update_rate
    EXPECT_NEAR(data.engine_config.update_rate, 50.0, 1e-6);

    // Проверяем новую геометрию
    ASSERT_EQ(data.geometry.size(), 1u);
    EXPECT_EQ(data.geometry[0].type, "box");
    EXPECT_NEAR(data.geometry[0].pose.x, 5.0, 1e-6);
    EXPECT_NEAR(data.geometry[0].size.x(), 3.0, 1e-6);

    std::remove(path.c_str());
}

TEST(SceneWriter, SaveEmptyGeometry) {
    // Сохранить пустой список примитивов
    const std::string yaml_content = R"(
s2:
  visualizer:
    enabled: false
  world:
    surface: "flat"
    geometry:
      - type: "box"
        pose: {x: 0, y: 0, z: 0, yaw: 0}
        size: {x: 1, y: 1, z: 1}
        color: "#808080"
  agents: []
)";
    auto path = make_temp_yaml(yaml_content);

    ASSERT_NO_THROW(SceneWriter::save_geometry(path, {}));

    auto reloaded = load_geometry_from_yaml(path);
    EXPECT_EQ(reloaded.size(), 0u);

    std::remove(path.c_str());
}

TEST(SceneWriter, ThrowsOnInvalidPath) {
    std::vector<WorldPrimitive> prims;
    EXPECT_THROW(
        SceneWriter::save_geometry("/nonexistent/path/scene.yaml", prims),
        std::runtime_error
    );
}

// ─── SimWorld geometry update test ─────────────────────────────

TEST(SimWorldUpdateGeometry, ReplacesGeometry) {
    SimWorld world;

    // Начальная геометрия
    WorldPrimitive w1; w1.type = "box"; w1.pose.x = 1.0;
    WorldPrimitive w2; w2.type = "sphere"; w2.pose.x = 2.0;
    world.add_static_primitive(w1);
    world.add_static_primitive(w2);
    EXPECT_EQ(world.static_geometry().size(), 2u);

    // Заменяем на одну сферу
    world.static_geometry().clear();
    WorldPrimitive w3; w3.type = "cylinder"; w3.pose.x = 5.0; w3.radius = 0.7;
    world.add_static_primitive(w3);

    ASSERT_EQ(world.static_geometry().size(), 1u);
    EXPECT_EQ(world.static_geometry()[0].type, "cylinder");
    EXPECT_NEAR(world.static_geometry()[0].pose.x, 5.0, 1e-9);
    EXPECT_NEAR(world.static_geometry()[0].radius, 0.7, 1e-9);
}

TEST(SimWorldUpdateGeometry, ClearThenAdd) {
    SimWorld world;
    WorldPrimitive p; p.type = "box"; p.size = Vec3{2, 2, 2};
    world.add_static_primitive(p);
    EXPECT_EQ(world.static_geometry().size(), 1u);

    // Полностью очищаем
    world.static_geometry().clear();
    EXPECT_EQ(world.static_geometry().size(), 0u);

    // Добавляем новую
    WorldPrimitive p2; p2.type = "sphere"; p2.radius = 1.5;
    world.add_static_primitive(p2);
    EXPECT_EQ(world.static_geometry().size(), 1u);
    EXPECT_EQ(world.static_geometry()[0].type, "sphere");
}

// ─── SceneWriter::save_agents Tests ────────────────────────────

TEST(SceneWriterAgents, SaveAndReloadAgents) {
    // Исходная сцена с одним агентом
    const std::string yaml_content = R"(
s2:
  update_rate: 100
  visualizer:
    enabled: false
  world:
    surface: "flat"
    geometry: []
  agents:
    - name: "old_robot"
      pose: {x: 0.0, y: 0.0, z: 0.0, yaw: 0.0}
)";
    auto path = make_temp_yaml(yaml_content);

    // Новый список агентов (JSON)
    nlohmann::json agents = nlohmann::json::array();
    {
        nlohmann::json a;
        a["name"] = "robot_0";
        a["domain_id"] = 0;
        nlohmann::json pose;
        pose["x"] = 3.5;
        pose["y"] = -1.0;
        pose["z"] = 0.0;
        pose["yaw"] = 1.57;
        a["pose"] = pose;
        nlohmann::json visual;
        visual["type"] = "box";
        visual["color"] = "#FF6B35";
        a["visual"] = visual;
        agents.push_back(a);
    }

    ASSERT_NO_THROW(SceneWriter::save_agents(path, agents));

    // Перечитываем и проверяем через SceneLoader
    auto data = SceneLoader::load(path);
    ASSERT_EQ(data.agents.size(), 1u);
    EXPECT_EQ(data.agents[0].name, "robot_0");
    EXPECT_NEAR(data.agents[0].world_pose.x, 3.5, 1e-6);
    EXPECT_NEAR(data.agents[0].world_pose.y, -1.0, 1e-6);
    EXPECT_NEAR(data.agents[0].world_pose.yaw, 1.57, 1e-4);

    std::remove(path.c_str());
}

TEST(SceneWriterAgents, AgentsPreserveGeometry) {
    // Проверяем что сохранение агентов не ломает геометрию
    const std::string yaml_content = R"(
s2:
  update_rate: 50
  visualizer:
    enabled: false
  world:
    surface: "flat"
    geometry:
      - type: "box"
        pose: {x: 5.0, y: 5.0, z: 0.5, yaw: 0.0}
        size: {x: 2.0, y: 2.0, z: 1.0}
        color: "#808080"
  agents: []
)";
    auto path = make_temp_yaml(yaml_content);

    // Добавляем одного агента
    nlohmann::json agents = nlohmann::json::array();
    nlohmann::json a;
    a["name"] = "new_robot";
    a["domain_id"] = 1;
    nlohmann::json pose;
    pose["x"] = 1.0; pose["y"] = 2.0; pose["z"] = 0.0; pose["yaw"] = 0.0;
    a["pose"] = pose;
    agents.push_back(a);

    ASSERT_NO_THROW(SceneWriter::save_agents(path, agents));

    // Агенты сохранены
    auto data = SceneLoader::load(path);
    ASSERT_EQ(data.agents.size(), 1u);
    EXPECT_EQ(data.agents[0].name, "new_robot");

    // Геометрия сохранена
    ASSERT_EQ(data.geometry.size(), 1u);
    EXPECT_EQ(data.geometry[0].type, "box");
    EXPECT_NEAR(data.geometry[0].pose.x, 5.0, 1e-6);

    // update_rate сохранён
    EXPECT_NEAR(data.engine_config.update_rate, 50.0, 1e-6);

    std::remove(path.c_str());
}

TEST(SceneWriterAgents, SaveEmptyAgents) {
    // Сохранить пустой список агентов
    const std::string yaml_content = R"(
s2:
  visualizer:
    enabled: false
  world:
    surface: "flat"
    geometry: []
  agents:
    - name: "robot_to_delete"
      pose: {x: 0, y: 0, z: 0, yaw: 0}
)";
    auto path = make_temp_yaml(yaml_content);

    ASSERT_NO_THROW(SceneWriter::save_agents(path, nlohmann::json::array()));

    auto data = SceneLoader::load(path);
    EXPECT_EQ(data.agents.size(), 0u);

    std::remove(path.c_str());
}

} // namespace s2
