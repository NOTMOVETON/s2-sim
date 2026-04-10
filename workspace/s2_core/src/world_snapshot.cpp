#include <s2/world_snapshot.hpp>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <cmath>

namespace s2 {

namespace {

nlohmann::json pose_to_json(const Pose3D& pose) {
    return {
        {"x",     pose.x},
        {"y",     pose.y},
        {"z",     pose.z},
        {"roll",  pose.roll},
        {"pitch", pose.pitch},
        {"yaw",   pose.yaw}
    };
}

nlohmann::json velocity_to_json(const Velocity& vel) {
    return {
        {"vx", vel.linear.x()},
        {"vy", vel.linear.y()},
        {"vz", vel.linear.z()},
        {"wx", vel.angular.x()},
        {"wy", vel.angular.y()},
        {"wz", vel.angular.z()}
    };
}


nlohmann::json visual_to_json(const VisualDesc& visual) {
    return {
        {"type", visual.type},
        {"size", {visual.size.x(), visual.size.y(), visual.size.z()}},
        {"radius", visual.radius},
        {"height", visual.height},
        {"color", visual.color}
    };
}

nlohmann::json zone_shape_to_json(const ZoneShape& shape) {
    nlohmann::json j;

    std::string shape_type;
    switch (shape.type) {
        case ZoneShapeType::SPHERE:   shape_type = "sphere"; break;
        case ZoneShapeType::AABB:     shape_type = "aabb"; break;
        case ZoneShapeType::INFINITE: shape_type = "infinite"; break;
    }

    j["shape_type"] = shape_type;
    j["center"] = {
        {"x", shape.center.x()},
        {"y", shape.center.y()},
        {"z", shape.center.z()}
    };

    if (shape.type == ZoneShapeType::SPHERE) {
        j["radius"] = shape.radius;
    } else if (shape.type == ZoneShapeType::AABB) {
        j["size"] = nlohmann::json::array({shape.half_size.x() * 2, shape.half_size.y() * 2, shape.half_size.z() * 2});
    }

    return j;
}

nlohmann::json agent_snapshot_to_json(const AgentSnapshot& agent) {
    nlohmann::json j;
    j["id"] = agent.id;
    j["name"] = agent.name;
    j["pose"] = pose_to_json(agent.pose);
    j["velocity"] = velocity_to_json(agent.velocity);
    j["visual"] = visual_to_json(agent.visual);
    j["color"] = agent.visual.color;
    j["battery_level"] = agent.battery_level;
    j["effective_speed_scale"] = agent.effective_speed_scale;
    j["motion_locked"] = agent.motion_locked;
    j["held_objects"] = agent.held_objects;

    if (!agent.kinematic_frames.empty()) {
        nlohmann::json frames = nlohmann::json::array();
        for (const auto& f : agent.kinematic_frames) {
            nlohmann::json fe = {{"name", f.name}, {"pose", pose_to_json(f.world_pose)}};
            if (!f.visual.type.empty()) {
                nlohmann::json vis;
                vis["type"]  = f.visual.type;
                vis["color"] = f.visual.color;
                if (f.visual.type == "box") {
                    vis["sx"] = f.visual.sx;
                    vis["sy"] = f.visual.sy;
                    vis["sz"] = f.visual.sz;
                } else if (f.visual.type == "cylinder") {
                    vis["radius"] = f.visual.radius;
                    vis["length"] = f.visual.length;
                } else if (f.visual.type == "sphere") {
                    vis["radius"] = f.visual.radius;
                }
                fe["visual"] = vis;
            }
            frames.push_back(fe);
        }
        j["kinematic_frames"] = frames;
    }

    return j;
}

nlohmann::json prop_snapshot_to_json(const PropSnapshot& prop) {
    nlohmann::json j;
    j["id"] = prop.id;
    j["type"] = prop.type;
    j["pose"] = pose_to_json(prop.pose);
    j["visual"] = visual_to_json(prop.visual);
    j["movable"] = prop.movable;
    if (prop.attached_to_agent.has_value()) {
        j["attached_to_agent"] = prop.attached_to_agent.value();
    } else {
        j["attached_to_agent"] = nullptr;
    }
    return j;
}

nlohmann::json actor_snapshot_to_json(const ActorSnapshot& actor) {
    nlohmann::json j;
    j["id"] = actor.id;
    j["name"] = actor.name;
    j["pose"] = pose_to_json(actor.pose);
    j["visual"] = visual_to_json(actor.visual);
    j["state"] = actor.state;
    return j;
}

nlohmann::json zone_snapshot_to_json(const ZoneSnapshot& zone) {
    nlohmann::json j;
    j["id"] = zone.id;
    j["enabled"] = zone.enabled;
    j["shape"] = zone_shape_to_json(zone.shape);
    j["agents_inside"] = zone.agents_inside;
    return j;
}

nlohmann::json geometry_snapshot_to_json(const GeometrySnapshot& geom) {
    nlohmann::json j;
    j["type"] = geom.type;
    j["x"] = geom.x;
    j["y"] = geom.y;
    j["z"] = geom.z;
    j["sx"] = geom.sx;
    j["sy"] = geom.sy;
    j["sz"] = geom.sz;
    j["radius"] = geom.radius;
    j["height"] = geom.height;
    j["color"] = geom.color;
    return j;
}

} // anonymous namespace

nlohmann::json snapshot_to_json(const WorldSnapshot& snapshot, bool include_geometry, bool include_plugins) {
    nlohmann::json j;
    j["sim_time"] = snapshot.sim_time;
    j["paused"] = snapshot.paused;

    if (include_plugins) {
        // Данные плагинов
        nlohmann::json plugins_json = nlohmann::json::object();
        for (const auto& [agent_id, plugin_map] : snapshot.plugins_data) {
            nlohmann::json agent_plugins = nlohmann::json::object();
            for (const auto& [plugin_type, json_str] : plugin_map) {
                // Пытаемся распарсить строку как JSON, если это валидный JSON
                try {
                    agent_plugins[plugin_type] = nlohmann::json::parse(json_str);
                } catch (...) {
                    // Если невалидный — кладём как строку
                    agent_plugins[plugin_type] = json_str;
                }
            }
            plugins_json[agent_id] = agent_plugins;
        }
        j["plugins_data"] = plugins_json;

        // Схемы входных данных плагинов
        nlohmann::json input_schemas_json = nlohmann::json::object();
        for (const auto& [agent_id, schema_str] : snapshot.plugin_inputs_schemas) {
            try {
                input_schemas_json[agent_id] = nlohmann::json::parse(schema_str);
            } catch (...) {
                input_schemas_json[agent_id] = schema_str;
            }
        }
        j["plugin_inputs_schemas"] = input_schemas_json;
    }

    nlohmann::json agents_json = nlohmann::json::array();
    for (const auto& agent : snapshot.agents) {
        agents_json.push_back(agent_snapshot_to_json(agent));
    }
    j["agents"] = agents_json;

    nlohmann::json props_json = nlohmann::json::array();
    for (const auto& prop : snapshot.props) {
        props_json.push_back(prop_snapshot_to_json(prop));
    }
    j["props"] = props_json;

    nlohmann::json actors_json = nlohmann::json::array();
    for (const auto& actor : snapshot.actors) {
        actors_json.push_back(actor_snapshot_to_json(actor));
    }
    j["actors"] = actors_json;

    nlohmann::json zones_json = nlohmann::json::array();
    for (const auto& zone : snapshot.zones) {
        zones_json.push_back(zone_snapshot_to_json(zone));
    }
    j["zones"] = zones_json;

    if (include_geometry) {
        nlohmann::json geometry_json = nlohmann::json::array();
        for (const auto& geom : snapshot.geometry) {
            geometry_json.push_back(geometry_snapshot_to_json(geom));
        }
        j["geometry"] = geometry_json;
    }

    return j;
}

} // namespace s2