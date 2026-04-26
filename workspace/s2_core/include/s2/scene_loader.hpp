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
#include <s2/zone.hpp>
#include <s2/zone_spawn_system.hpp>
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
    std::vector<Zone> zones;
    std::vector<ZoneSpawnSystem::ZoneTemplate> zone_templates;  ///< Шаблоны для ZoneSpawnSystem (ZONE-08)
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
    static ZoneShape parse_zone_shape(const YAML::Node& node);
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
                agent.has_collision = true;
            }

            if (agent_node["max_slope_deg"]) {
                agent.max_slope_rad =
                    agent_node["max_slope_deg"].as<double>(0.0) * M_PI / 180.0;
            }

            if (agent_node["max_step_height"]) {
                agent.max_step_height =
                    agent_node["max_step_height"].as<double>(0.0);
            }

            if (agent_node["visual"]) {
                agent.visual = parse_visual(agent_node["visual"]);
            }

            // Capabilities агента (для матчинга с требованиями эффектов зон)
            if (agent_node["capabilities"]) {
                const auto& caps = agent_node["capabilities"];
                if (caps.IsSequence()) {
                    for (const auto& cap : caps) {
                        agent.capabilities.insert(cap.as<std::string>());
                    }
                }
            }

            // Инициализация компонентов по capabilities выполняется плагинами (задача 32).
            // SceneLoader отвечает только за структуру агента, не за доменные компоненты.

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

            // Валидация: не более одного ACTUATION-плагина на агента (D-11)
            {
                int actuation_count = 0;
                for (const auto& plugin : agent.plugins)
                    if (plugin->role() == PluginRole::ACTUATION)
                        actuation_count++;
                if (actuation_count > 1) {
                    throw std::runtime_error(
                        "Агент '" + agent.name + "' имеет " +
                        std::to_string(actuation_count) +
                        " ACTUATION-плагина. Допустим только один.");
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

                // Коллизионный шейп из URDF (приоритет выше YAML collision:)
                // Читаем <link name="base_link"><collision><geometry>
                auto urdf_col = load_urdf_collision(urdf_path);
                if (urdf_col.has_value()) {
                    agent.bounding = urdf_col.value();
                    agent.has_collision = true;
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

            // owned_zones — спавнить как зоны с attached_to_entity_id = agent.id (ZONE-09, D-17)
            if (const auto& owned = agent_node["owned_zones"]) {
                for (const auto& zn_owned : owned) {
                    Zone oz;
                    oz.id = zn_owned["id"].as<std::string>(
                        agent.name + "_zone_" + std::to_string(scene.zones.size()));
                    oz.color   = zn_owned["color"].as<std::string>("#4488FF");
                    oz.opacity = zn_owned["opacity"].as<double>(0.3);
                    oz.visible = zn_owned["visible"].as<bool>(true);
                    if (zn_owned["shape"]) oz.shape = parse_zone_shape(zn_owned["shape"]);
                    oz.attached_to_entity_id = std::to_string(agent.id);
                    if (zn_owned["attached_to_link"])
                        oz.attached_to_link = zn_owned["attached_to_link"].as<std::string>();
                    // Эффекты — по тому же паттерну что и zone.effects
                    if (zn_owned["effects"]) {
                        for (const auto& en : zn_owned["effects"]) {
                            Zone::EffectDesc desc;
                            desc.type   = en["type"].as<std::string>("");
                            desc.params = en["params"] ? en["params"] : YAML::Node{};
                            oz.effects.push_back(std::move(desc));
                        }
                    }
                    scene.zones.push_back(std::move(oz));
                }
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

    // ── Зоны ──
    if (const auto& zones_node = root["s2"]["zones"]) {
        for (const auto& zn : zones_node) {
            Zone z;
            z.id             = zn["id"].as<std::string>("");
            z.enabled        = zn["enabled"].as<bool>(true);
            z.color          = zn["color"].as<std::string>("#4488FF");
            z.opacity        = zn["opacity"].as<double>(0.3);
            z.visible        = zn["visible"].as<bool>(true);
            z.label          = zn["label"].as<std::string>("");
            z.detection_mode = zn["detection_mode"].as<std::string>("center");

            // Конвертировать строку detection_mode в enum (per D-12)
            {
                const std::string& dm_str = z.detection_mode;
                if (dm_str == "bounding")        z.detection_mode_enum = DetectionMode::BOUNDING;
                else if (dm_str == "per_link")   z.detection_mode_enum = DetectionMode::PER_LINK;
                else                              z.detection_mode_enum = DetectionMode::CENTER;
            }

            // Lifecycle секция (ZONE-04)
            if (const auto& lc = zn["lifecycle"]) {
                z.lifecycle.initial_strength  = lc["initial_strength"].as<double>(1.0);
                z.lifecycle.growth_rate       = lc["growth_rate"].as<double>(0.0);
                z.lifecycle.max_strength      = lc["max_strength"].as<double>(1.0);
                z.lifecycle.decay_delay       = lc["decay_delay"].as<double>(0.0);
                z.lifecycle.decay_rate        = lc["decay_rate"].as<double>(0.0);
                z.lifecycle.remove_threshold  = lc["remove_threshold"].as<double>(0.05);
                z.strength = z.lifecycle.initial_strength;  // Установить начальную силу
            }

            // Self-destruct policy (ZONE-07)
            if (const auto& sd = zn["self_destruct_policy"]) {
                std::string sdt = sd.as<std::string>("none");
                if (sdt == "on_any_contact")
                    z.self_destruct.type = SelfDestructPolicy::Type::ON_ANY_CONTACT;
                else if (sdt == "on_effect_applied")
                    z.self_destruct.type = SelfDestructPolicy::Type::ON_EFFECT_APPLIED;
                else
                    z.self_destruct.type = SelfDestructPolicy::Type::NONE;
            }

            if (zn["shape"]) {
                z.shape = parse_zone_shape(zn["shape"]);
            }

            // Привязка к актору по имени (ищем в уже загруженных акторах)
            if (zn["attached_to"]) {
                std::string attach_name = zn["attached_to"].as<std::string>("");
                for (const auto& actor : scene.actors) {
                    if (actor.name == attach_name) {
                        z.attached_to_actor = actor.id;
                        break;
                    }
                }
            }

            // Эффекты — без plugin (плагины создаются ZoneSystem через фабрику)
            if (zn["effects"]) {
                for (const auto& en : zn["effects"]) {
                    Zone::EffectDesc desc;
                    desc.type    = en["type"].as<std::string>("");
                    desc.enabled = en["enabled"].as<bool>(true);

                    // Тип эффекта
                    if (en["effect_type"]) {
                        std::string et = en["effect_type"].as<std::string>("modifier");
                        if (et == "modifier")   desc.effect_type = EffectType::MODIFIER;
                        else if (et == "continuous") desc.effect_type = EffectType::CONTINUOUS;
                        else if (et == "mutation")   desc.effect_type = EffectType::MUTATION;
                        else if (et == "sensor")     desc.effect_type = EffectType::SENSOR;
                    }

                    // Capabilities
                    if (en["required_capabilities"] && en["required_capabilities"].IsSequence()) {
                        for (const auto& cap : en["required_capabilities"]) {
                            desc.required_capabilities.push_back(cap.as<std::string>());
                        }
                    }

                    desc.params = en["params"] ? en["params"] : YAML::Node{};
                    z.effects.push_back(std::move(desc));
                }
            }

            scene.zones.push_back(std::move(z));
        }
    }

    // ── Zone Templates для ZoneSpawnSystem (ZONE-08, D-14) ──
    if (const auto& templates_node = root["s2"]["zone_templates"]) {
        for (const auto& tmpl_kv : templates_node) {
            std::string tmpl_name = tmpl_kv.first.as<std::string>();
            const auto& tmpl_node = tmpl_kv.second;

            ZoneSpawnSystem::ZoneTemplate tmpl;
            tmpl.name = tmpl_name;

            // Парсить spawn_cmd (shape + effects + color + ...)
            if (tmpl_node["shape"])
                tmpl.spawn_cmd.shape = parse_zone_shape(tmpl_node["shape"]);
            tmpl.spawn_cmd.color   = tmpl_node["color"].as<std::string>("#4488FF");
            tmpl.spawn_cmd.opacity = tmpl_node["opacity"].as<double>(0.3);
            tmpl.spawn_cmd.visible = tmpl_node["visible"].as<bool>(true);
            if (tmpl_node["label"])
                tmpl.spawn_cmd.label = tmpl_node["label"].as<std::string>("");
            if (tmpl_node["effects"]) {
                for (const auto& en : tmpl_node["effects"]) {
                    tmpl.spawn_cmd.effects.push_back(en["type"].as<std::string>(""));
                }
            }

            // Парсить spawn_trigger
            if (const auto& st = tmpl_node["spawn_trigger"]) {
                std::string tt = st["type"].as<std::string>("timer");
                if (tt == "timer") {
                    tmpl.trigger_type = ZoneSpawnSystem::ZoneTemplate::TriggerType::TIMER;
                    tmpl.timer.delay_seconds = st["delay"].as<double>(0.0);
                } else if (tt == "event") {
                    tmpl.trigger_type = ZoneSpawnSystem::ZoneTemplate::TriggerType::EVENT;
                    tmpl.event.event_type    = st["event_type"].as<std::string>("");
                    tmpl.event.source_entity = st["source_entity"].as<std::string>("");
                } else if (tt == "state_change") {
                    tmpl.trigger_type = ZoneSpawnSystem::ZoneTemplate::TriggerType::STATE_CHANGE;
                    tmpl.state_change.actor_id    = st["actor_id"].as<uint32_t>(0);
                    tmpl.state_change.target_state = st["state_value"].as<std::string>("");
                }
            }

            scene.zone_templates.push_back(std::move(tmpl));
        }
    }

    return scene;
}

inline Pose3D SceneLoader::parse_pose(const YAML::Node& node) {
    Pose3D pose;
    pose.x     = node["x"].as<double>(0.0);
    pose.y     = node["y"].as<double>(0.0);
    pose.z     = node["z"].as<double>(0.0);
    pose.yaw   = node["yaw"].as<double>(0.0);
    pose.pitch = node["pitch"].as<double>(0.0);
    pose.roll  = node["roll"].as<double>(0.0);
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
        if (geom["height"]) {
            prim.height = geom["height"].as<double>(1.0);
        } else if (prim.type == "cylinder" && prim.size.z() > 0.0) {
            // Обратная совместимость: если height не задан явно — берём size.z
            prim.height = prim.size.z();
        }
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

inline ZoneShape SceneLoader::parse_zone_shape(const YAML::Node& node) {
    ZoneShape s;
    std::string type_str = node["type"].as<std::string>("sphere");

    if (type_str == "sphere") {
        s.type   = ZoneShapeType::SPHERE;
        s.radius = node["radius"].as<double>(1.0);
        if (node["center"]) {
            const auto& c = node["center"];
            s.center = Vec3{c["x"].as<double>(0.0), c["y"].as<double>(0.0), c["z"].as<double>(0.0)};
        }
    } else if (type_str == "aabb") {
        s.type = ZoneShapeType::AABB;
        if (node["center"]) {
            const auto& c = node["center"];
            s.center = Vec3{c["x"].as<double>(0.0), c["y"].as<double>(0.0), c["z"].as<double>(0.0)};
        }
        if (node["half_size"]) {
            const auto& hs = node["half_size"];
            s.half_size = Vec3{hs["x"].as<double>(1.0), hs["y"].as<double>(1.0), hs["z"].as<double>(1.0)};
        }
    } else if (type_str == "cylinder") {
        s.type        = ZoneShapeType::CYLINDER;
        s.radius      = node["radius"].as<double>(1.0);
        s.half_height = node["half_height"].as<double>(1.0);
        if (node["center"]) {
            const auto& c = node["center"];
            s.center = Vec3{c["x"].as<double>(0.0), c["y"].as<double>(0.0), c["z"].as<double>(0.0)};
        }
    } else if (type_str == "infinite") {
        s.type = ZoneShapeType::INFINITE;
    }

    return s;
}

} // namespace s2