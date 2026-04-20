#pragma once

#include <s2/types.hpp>
#include <s2/kinematic_tree.hpp>
#include <vector>
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace s2 {

/** Снимок одного звена кинематического дерева в мировых координатах */
struct LinkFrameSnapshot {
    std::string name;       ///< Имя звена (например "arm", "gps_right_link")
    Pose3D      world_pose; ///< Поза звена в мировых координатах
    LinkVisual  visual;     ///< Визуальная геометрия звена (из URDF)
};

/** Снимок агента для визуализатора */
struct AgentSnapshot {
    AgentId id;
    std::string name;
    Pose3D pose;
    Velocity velocity;          ///< Скорость тела (actuation, локальные координаты)
    Vec3 velocity_addition;     ///< Аддитивная скорость от зон (мировые координаты)
    VisualDesc visual;

    /// Агрегированное физическое состояние ядра (из SharedState::effective()).
    double effective_speed_scale{1.0};
    bool motion_locked{false};

    /// Расширяемые данные плагинов.
    /// Каждый плагин вносит свои поля через IAgentPlugin::contribute_snapshot().
    /// Пример: BatteryPlugin добавляет battery_level и battery_charging.
    nlohmann::json extra = nlohmann::json::object();

    /// Позы всех звеньев кинематического дерева (кроме корня) в мировых координатах.
    /// Пустой — если kinematic_tree не задано.
    std::vector<LinkFrameSnapshot> kinematic_frames;

    /// Коллизионный шейп (для визуализации в UI).
    bool        has_collision{false};
    std::string bounding_type;        ///< "sphere" или "box"
    double      bounding_radius{0.5}; ///< для sphere [м]
    Vec3        bounding_size;        ///< для box: full extents (half-extents * 2) [м]
};

/** Снимок пропа для визуализатора */
struct PropSnapshot {
    ActorId id;
    std::string type;
    Pose3D pose;
    VisualDesc visual;
    bool movable = false;
    std::optional<AgentId> attached_to_agent;
};

/** Снимок актора для визуализатора */
struct ActorSnapshot {
    ActorId id;
    std::string name;
    Pose3D pose;
    VisualDesc visual;
    ActorState state;
};

/** Снимок зоны для визуализатора */
struct ZoneSnapshot {
    ZoneId id;
    bool enabled{true};

    // Геометрия зоны в плоском виде (удобно для JSON-сериализации)
    ZoneShapeType shape_type{ZoneShapeType::SPHERE};
    Vec3 center{Vec3::Zero()};
    double radius{1.0};
    Vec3 half_size{Vec3::Zero()};
    double half_height{1.0};

    // Визуальные параметры
    std::string color{"#4488FF"};
    double opacity{0.3};
    bool visible{true};
    std::string label;

    std::vector<AgentId> agents_inside;
};

/** Статический примитив для визуализатора (упрощённая версия WorldPrimitive) */
struct GeometrySnapshot {
    std::string type;         /// "box", "sphere", "cylinder"
    double x{0}, y{0}, z{0};  /// позиция
    double yaw{0}, pitch{0}, roll{0}; /// ориентация (ZYX Euler, радианы)
    double sx{1}, sy{1}, sz{1}; /// размер
    double radius{0.5};       /// радиус для sphere/cylinder
    double height{1.0};       /// высота для cylinder
    std::string color{"#808080"};
};

/** Моментальный снимок всего состояния мира */
struct WorldSnapshot {
    double sim_time = 0.0;
    bool paused = false;

    std::vector<AgentSnapshot> agents;
    std::vector<PropSnapshot> props;
    std::vector<ActorSnapshot> actors;
    std::vector<ZoneSnapshot> zones;

    /** Данные плагинов: agent_id -> { plugin_type -> json_string } */
    std::map<std::string, std::map<std::string, std::string>> plugins_data;

    /** Схемы входных данных плагинов: agent_id -> JSON-string { plugin_type -> schema } */
    std::map<std::string, std::string> plugin_inputs_schemas;

    /** Статическая геометрия — заполняется только при первом подключении клиента */
    std::vector<GeometrySnapshot> geometry;
};

/**
 * Сериализует WorldSnapshot в JSON.
 * @param snapshot        Снимок мира
 * @param include_geometry  Если true — включить статическую геометрию
 * @return JSON-объект
 */
nlohmann::json snapshot_to_json(const WorldSnapshot& snapshot, bool include_geometry = false, bool include_plugins = true);

} // namespace s2
