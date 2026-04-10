#pragma once

/**
 * @file scene_loader.hpp
 * SceneLoader — загрузка сцены из YAML файла.
 *
 * v2: heightmap (flat), статическая геометрия, агенты, плагины, geo_origin.
 * YAML-cpp уже установлен в Dockerfile.
 */

#include <s2/sim_engine.hpp>
#include <s2/world.hpp>
#include <s2/geo_origin.hpp>
#include <s2/agent.hpp>
#include <s2/urdf_loader.hpp>
#include <yaml-cpp/yaml.h>

#include <cmath>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Forward-declare для IAgentPlugin
namespace s2 { namespace plugins { class IAgentPlugin; } }

namespace s2 {

struct TransportConfig {
    std::string type           = "ros2";  // "ros2" | "stub"
    int         default_domain_id = 0;
};

struct VizConfig {
    bool enabled = true;
    int  port    = 8080;
};

/// Результат загрузки сцены.
struct SceneData {
    SimEngine::Config engine_config;
    TransportConfig   transport_config;
    VizConfig         viz_config;
    Heightmap heightmap;
    GeoOrigin geo_origin;    ///< Начальная LLA точка сцены
    std::vector<WorldPrimitive> geometry;
    std::vector<Agent> agents;
    std::vector<Prop> props;
    std::vector<Actor> actors;
};

/// Загрузчик сцены из YAML.
class SceneLoader {
public:
    /// Загрузить сцену из YAML файла.
    /// @param yaml_path Путь к YAML файлу
    /// @param plugin_factory Опциональная фабрика для создания плагинов.
    ///        Принимает (type, yaml_node) и возвращает unique_ptr<IAgentPlugin>
    using PluginFactory = std::function<std::unique_ptr<plugins::IAgentPlugin>(
        const std::string& type, const YAML::Node& node)>;

    static SceneData load(const std::string& yaml_path,
                          PluginFactory plugin_factory = PluginFactory{});

private:
    static Pose3D parse_pose(const YAML::Node& node);
    static VisualDesc parse_visual(const YAML::Node& node);
    static CollisionShape parse_collision(const YAML::Node& node);
    static Heightmap parse_heightmap(const YAML::Node& node);
    static std::vector<WorldPrimitive> parse_geometry(const YAML::Node& node);
    static GeoOrigin parse_geo_origin(const YAML::Node& node);
};

// ─── Implementation ────────────────────────────────────────────────────

inline SceneData SceneLoader::load(const std::string& yaml_path,
                                   PluginFactory plugin_factory) {
    YAML::Node root = YAML::LoadFile(yaml_path);

    SceneData scene;

    // ── Параметры движка ──
    if (const auto& s2 = root["s2"]) {
        if (s2["update_rate"])    scene.engine_config.update_rate    = s2["update_rate"].as<double>();
        if (s2["viz_rate"])       scene.engine_config.viz_rate       = s2["viz_rate"].as<double>();
        if (s2["transport_rate"]) scene.engine_config.transport_rate = s2["transport_rate"].as<double>();

        if (const auto& tr = s2["transport"]) {
            if (tr["type"])               scene.transport_config.type              = tr["type"].as<std::string>();
            if (tr["default_domain_id"])  scene.transport_config.default_domain_id = tr["default_domain_id"].as<int>();
        }

        if (const auto& viz = s2["visualizer"]) {
            if (viz["enabled"]) scene.viz_config.enabled = viz["enabled"].as<bool>();
            if (viz["port"])    scene.viz_config.port    = viz["port"].as<int>();
        }
    }

    // ── Мир ──
    if (const auto& world = root["s2"]["world"]) {
        // Heightmap
        if (world["surface"]) {
            scene.heightmap = parse_heightmap(world["surface"]);
        }

        // Геометрия
        if (world["geometry"]) {
            scene.geometry = parse_geometry(world["geometry"]);
        }

        // GeoOrigin — единая LLA точка отсчёта на всю сцену
        if (world["geo_origin"]) {
            scene.geo_origin = parse_geo_origin(world["geo_origin"]);
        }
    }

    // ── Агенты ──
    if (const auto& agents = root["s2"]["agents"]) {
        AgentId id = 0;
        for (const auto& agent_node : agents) {
            Agent agent;
            agent.id = id++;

            // domain_id для изоляции в ROS2
            if (agent_node["domain_id"]) {
                agent.domain_id = agent_node["domain_id"].as<int>(0);
            }

            if (agent_node["name"]) {
                agent.name = agent_node["name"].as<std::string>();
            }

            if (agent_node["pose"]) {
                agent.world_pose = parse_pose(agent_node["pose"]);
            }

            if (agent_node["collision"]) {
                agent.bounding = parse_collision(agent_node["collision"]);
            }

            if (agent_node["visual"]) {
                agent.visual = parse_visual(agent_node["visual"]);
            }

            // Начальная скорость (если задана в конфиге)
            if (agent_node["velocity"]) {
                const auto& vel = agent_node["velocity"];
                double vx = vel["linear_x"].as<double>(0.0);
                double vy = vel["linear_y"].as<double>(0.0);
                double wz = vel["angular_z"].as<double>(0.0);
                agent.world_velocity.linear = Vec3{vx, vy, 0.0};
                agent.world_velocity.angular = Vec3{0.0, 0.0, wz};
            }

            // Плагины агента
            if (agent_node["plugins"] && plugin_factory) {
                for (const auto& plugin_node : agent_node["plugins"]) {
                    if (plugin_node["type"]) {
                        std::string type = plugin_node["type"].as<std::string>();
                        auto plugin = plugin_factory(type, plugin_node);
                        if (plugin) {
                            // Имя экземпляра плагина (для именованных топиков)
                            if (plugin_node["name"]) {
                                plugin->set_sensor_name(
                                    plugin_node["name"].as<std::string>());
                            }
                            // Переопределение выходного топика
                            if (plugin_node["topic"]) {
                                plugin->set_output_topic(
                                    plugin_node["topic"].as<std::string>());
                            }
                            // Переопределение частоты публикации
                            if (plugin_node["publish_rate_hz"]) {
                                plugin->set_base_rate(
                                    plugin_node["publish_rate_hz"].as<double>());
                            }
                            // Точка монтажа сенсора на теле агента
                            if (plugin_node["mount"]) {
                                plugin->set_mount_pose(
                                    parse_pose(plugin_node["mount"]));
                            }
                            agent.plugins.push_back(std::move(plugin));
                        }
                    }
                }
            }

            // Кинематическое дерево из URDF (имеет приоритет над links:)
            if (agent_node["urdf"]) {
                std::string urdf_rel = agent_node["urdf"].as<std::string>();
                // Путь относительно директории YAML-файла
                std::filesystem::path yaml_dir =
                    std::filesystem::path(yaml_path).parent_path();
                std::string urdf_path = (yaml_dir / urdf_rel).string();
                try {
                    auto tree = std::make_unique<KinematicTree>(
                        load_urdf(urdf_path));
                    if (!tree->empty())
                        agent.kinematic_tree = std::move(tree);
                } catch (const std::exception& e) {
                    throw std::runtime_error(
                        std::string("SceneLoader: ошибка загрузки URDF '")
                        + urdf_path + "': " + e.what());
                }
            }

            // Кинематическое дерево агента (links:) — если urdf: не задан
            if (!agent.kinematic_tree && agent_node["links"]) {
                auto tree = std::make_unique<KinematicTree>();
                for (const auto& link_node : agent_node["links"]) {
                    Link lk;
                    lk.name   = link_node["name"].as<std::string>("");
                    lk.parent = link_node["parent"].as<std::string>("");

                    if (link_node["origin"])
                        lk.origin = parse_pose(link_node["origin"]);

                    if (link_node["joint"]) {
                        const auto& jn = link_node["joint"];
                        std::string jtype = jn["type"].as<std::string>("fixed");

                        if      (jtype == "revolute")   lk.joint.type = JointType::REVOLUTE;
                        else if (jtype == "prismatic")  lk.joint.type = JointType::PRISMATIC;
                        else if (jtype == "continuous") lk.joint.type = JointType::CONTINUOUS;
                        else                            lk.joint.type = JointType::FIXED;

                        if (jn["axis"] && jn["axis"].IsSequence() &&
                            jn["axis"].size() == 3)
                        {
                            lk.joint.axis = Vec3(
                                jn["axis"][0].as<double>(0.0),
                                jn["axis"][1].as<double>(0.0),
                                jn["axis"][2].as<double>(1.0));
                        }

                        lk.joint.min   = jn["min"].as<double>(-M_PI);
                        lk.joint.max   = jn["max"].as<double>( M_PI);
                        lk.joint.value = jn["value"].as<double>(0.0);
                    }

                    if (!lk.name.empty())
                        tree->add_link(std::move(lk));
                }
                if (!tree->empty())
                    agent.kinematic_tree = std::move(tree);
            }

            scene.agents.push_back(std::move(agent));
        }
    }

    // ── Пропы ──
    if (const auto& props = root["s2"]["props"]) {
        ObjectId id = 0;
        for (const auto& prop_node : props) {
            Prop prop;
            prop.id = id++;

            if (prop_node["type"]) {
                prop.type = prop_node["type"].as<std::string>();
            }

            if (prop_node["pose"]) {
                prop.world_pose = parse_pose(prop_node["pose"]);
            }

            if (prop_node["movable"]) {
                prop.movable = prop_node["movable"].as<bool>(true);
            }

            if (prop_node["collision"]) {
                prop.collision = parse_collision(prop_node["collision"]);
            }

            if (prop_node["visual"]) {
                prop.visual = parse_visual(prop_node["visual"]);
            }

            scene.props.push_back(std::move(prop));
        }
    }

    // ── Акторы ──
    if (const auto& actors = root["s2"]["actors"]) {
        ActorId id = 0;
        for (const auto& actor_node : actors) {
            Actor actor;
            actor.id = id++;

            if (actor_node["name"]) {
                actor.name = actor_node["name"].as<std::string>();
            }

            if (actor_node["pose"]) {
                actor.world_pose = parse_pose(actor_node["pose"]);
            }

            if (actor_node["visual"]) {
                actor.visual = parse_visual(actor_node["visual"]);
            }

            scene.actors.push_back(std::move(actor));
        }
    }

    return scene;
}

inline Pose3D SceneLoader::parse_pose(const YAML::Node& node) {
    Pose3D pose;
    pose.x = node["x"].as<double>(0.0);
    pose.y = node["y"].as<double>(0.0);
    pose.z = node["z"].as<double>(0.0);
    pose.yaw = node["yaw"].as<double>(0.0);
    return pose;
}

inline ShapeType string_to_shape_type(const std::string& s) {
    if (s == "sphere") return ShapeType::SPHERE;
    if (s == "box") return ShapeType::BOX;
    if (s == "capsule") return ShapeType::CAPSULE;
    return ShapeType::SPHERE;
}

inline VisualDesc SceneLoader::parse_visual(const YAML::Node& node) {
    VisualDesc vis;
    if (node["type"]) vis.type = node["type"].as<std::string>("box");
    if (node["size"]) {
        if (node["size"].IsSequence()) {
            auto s = node["size"];
            vis.size = Vec3{s[0].as<double>(), s[1].as<double>(), s[2].as<double>()};
        } else if (node["size"]["x"]) {
            vis.size = Vec3{
                node["size"]["x"].as<double>(),
                node["size"]["y"].as<double>(),
                node["size"]["z"].as<double>()
            };
        }
    }
    if (node["color"]) vis.color = node["color"].as<std::string>("#FF6B35");
    return vis;
}

inline CollisionShape SceneLoader::parse_collision(const YAML::Node& node) {
    CollisionShape col;
    if (node["bounding"] && node["bounding"].IsMap()) {
        const auto& b = node["bounding"];
        if (b["type"]) col.type = string_to_shape_type(b["type"].as<std::string>());
        if (b["radius"]) col.radius = b["radius"].as<double>();
        if (b["height"]) col.height = b["height"].as<double>();
        if (b["size"] && b["size"].IsSequence()) {
            auto s = b["size"];
            col.size = Vec3{s[0].as<double>(), s[1].as<double>(), s[2].as<double>()};
        }
    }
    return col;
}

inline Heightmap SceneLoader::parse_heightmap(const YAML::Node& node) {
    if (node.IsScalar() && node.as<std::string>() == "flat") {
        return Heightmap::flat(40.0, 40.0, 0.0);
    }

    if (const auto& hm = node["path"]) {
        return Heightmap::flat(40.0, 40.0, 0.0);
    }

    if (const auto& hm = node) {
        double w = node["width"].as<double>(40.0);
        double h = node["height"].as<double>(40.0);
        double z = node["z"].as<double>(0.0);
        return Heightmap::flat(w, h, z);
    }

    return Heightmap::flat(40.0, 40.0, 0.0);
}

inline std::vector<WorldPrimitive> SceneLoader::parse_geometry(const YAML::Node& node) {
    std::vector<WorldPrimitive> prims;

    for (const auto& geom : node) {
        WorldPrimitive prim;

        if (geom["type"]) prim.type = geom["type"].as<std::string>("box");

        if (geom["pose"]) {
            prim.pose = parse_pose(geom["pose"]);
        }

        if (geom["size"]) {
            if (geom["size"].IsSequence()) {
                auto s = geom["size"];
                prim.size = Vec3{s[0].as<double>(), s[1].as<double>(), s[2].as<double>()};
            } else if (geom["size"]["x"]) {
                prim.size = Vec3{
                    geom["size"]["x"].as<double>(),
                    geom["size"]["y"].as<double>(),
                    geom["size"]["z"].as<double>()
                };
            }
        }

        if (geom["radius"]) prim.radius = geom["radius"].as<double>(0.5);
        if (geom["height"]) prim.height = geom["height"].as<double>(1.0);
        if (geom["color"]) prim.color = geom["color"].as<std::string>("#808080");

        prims.push_back(std::move(prim));
    }

    return prims;
}

inline GeoOrigin SceneLoader::parse_geo_origin(const YAML::Node& node) {
    GeoOrigin origin;
    if (node["lat"]) origin.lat = node["lat"].as<double>();
    if (node["lon"]) origin.lon = node["lon"].as<double>();
    if (node["alt"]) origin.alt = node["alt"].as<double>(0.0);
    return origin;
}

} // namespace s2