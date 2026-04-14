#pragma once

/**
 * @file scene_writer.hpp
 * SceneWriter — сохранение геометрии сцены обратно в YAML-файл.
 *
 * Загружает существующий YAML, заменяет секцию s2.world.geometry,
 * остальное содержимое сохраняет нетронутым.
 */

#include <s2/world.hpp>
#include <yaml-cpp/yaml.h>

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace s2 {

class SceneWriter {
public:
    /**
     * @brief Сохранить текущую геометрию поверх оригинального YAML-файла.
     *
     * Загружает yaml_path, заменяет секцию s2.world.geometry на prims,
     * записывает обратно. Остальные секции (agents, props, etc.) не трогает.
     *
     * @param yaml_path  Путь к YAML-файлу сцены
     * @param prims      Список примитивов для записи
     * @throws std::runtime_error  При ошибке чтения/записи файла
     */
    static void save_geometry(const std::string& yaml_path,
                              const std::vector<WorldPrimitive>& prims);
};

// ─── Implementation ────────────────────────────────────────────

inline void SceneWriter::save_geometry(const std::string& yaml_path,
                                       const std::vector<WorldPrimitive>& prims)
{
    // Загружаем существующий файл
    YAML::Node root;
    try {
        root = YAML::LoadFile(yaml_path);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error(
            std::string("SceneWriter: ошибка чтения YAML '") + yaml_path + "': " + e.what());
    }

    // Строим новую geometry-секцию
    YAML::Node geom_seq(YAML::NodeType::Sequence);
    for (const auto& prim : prims) {
        YAML::Node entry;
        entry["type"] = prim.type;

        // Поза
        YAML::Node pose_node;
        pose_node["x"]     = prim.pose.x;
        pose_node["y"]     = prim.pose.y;
        pose_node["z"]     = prim.pose.z;
        pose_node["yaw"]   = prim.pose.yaw;
        pose_node["pitch"] = prim.pose.pitch;
        pose_node["roll"]  = prim.pose.roll;
        entry["pose"] = pose_node;

        // Геометрические параметры по типу
        if (prim.type == "box") {
            YAML::Node size_node;
            size_node["x"] = prim.size.x();
            size_node["y"] = prim.size.y();
            size_node["z"] = prim.size.z();
            entry["size"] = size_node;
        } else if (prim.type == "cylinder") {
            entry["radius"] = prim.radius;
            entry["height"] = prim.height;
        } else if (prim.type == "sphere") {
            entry["radius"] = prim.radius;
        }

        entry["color"] = prim.color;
        geom_seq.push_back(entry);
    }

    // Устанавливаем секцию s2.world.geometry
    if (!root["s2"]) {
        throw std::runtime_error("SceneWriter: YAML не содержит секцию 's2'");
    }
    if (!root["s2"]["world"]) {
        root["s2"]["world"] = YAML::Node(YAML::NodeType::Map);
    }
    root["s2"]["world"]["geometry"] = geom_seq;

    // Записываем обратно
    std::ofstream out(yaml_path);
    if (!out.is_open()) {
        throw std::runtime_error(
            std::string("SceneWriter: не удалось открыть файл для записи: ") + yaml_path);
    }
    out << root;
    if (out.fail()) {
        throw std::runtime_error(
            std::string("SceneWriter: ошибка записи в файл: ") + yaml_path);
    }
}

} // namespace s2
