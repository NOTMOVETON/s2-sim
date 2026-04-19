#pragma once

/**
 * @file scene_writer.hpp
 * SceneWriter — сохранение сцены обратно в YAML-файл.
 *
 * Загружает существующий YAML, заменяет нужные секции,
 * остальное содержимое сохраняет нетронутым.
 */

#include <s2/world.hpp>
#include <nlohmann/json.hpp>
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

    /**
     * @brief Сохранить список агентов поверх оригинального YAML-файла.
     *
     * Загружает yaml_path, заменяет секцию s2.agents на agents_json,
     * записывает обратно. Остальные секции (world.geometry, transport, etc.) не трогает.
     *
     * Формат agents_json — JSON-массив агентов:
     * [{"name":"robot_0","domain_id":0,"pose":{...},"visual":{...},"plugins":[...]}, ...]
     *
     * @param yaml_path    Путь к YAML-файлу сцены
     * @param agents_json  JSON-массив агентов
     * @throws std::runtime_error  При ошибке чтения/записи или парсинга JSON
     */
    static void save_agents(const std::string& yaml_path,
                            const nlohmann::json& agents_json);

private:
    // Рекурсивное преобразование nlohmann::json → YAML::Node (для сохранения в block-стиле)
    static YAML::Node json_to_yaml(const nlohmann::json& j);
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

// ─── Implementation: json_to_yaml ──────────────────────────────────────────

inline YAML::Node SceneWriter::json_to_yaml(const nlohmann::json& j)
{
    if (j.is_null())
        return YAML::Node(YAML::NodeType::Null);

    if (j.is_boolean())
        return YAML::Node(j.get<bool>());

    if (j.is_number_integer())
        return YAML::Node(j.get<int64_t>());

    if (j.is_number_float())
        return YAML::Node(j.get<double>());

    if (j.is_string())
        return YAML::Node(j.get<std::string>());

    if (j.is_array())
    {
        YAML::Node node(YAML::NodeType::Sequence);
        for (const auto& item : j)
            node.push_back(json_to_yaml(item));
        return node;
    }

    if (j.is_object())
    {
        YAML::Node node(YAML::NodeType::Map);
        for (auto it = j.begin(); it != j.end(); ++it)
            node[it.key()] = json_to_yaml(it.value());
        return node;
    }

    return YAML::Node();
}

// ─── Implementation: save_agents ───────────────────────────────────────────

inline void SceneWriter::save_agents(const std::string& yaml_path,
                                     const nlohmann::json& agents_json)
{
    // Загружаем существующий файл
    YAML::Node root;
    try {
        root = YAML::LoadFile(yaml_path);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error(
            std::string("SceneWriter: ошибка чтения YAML '") + yaml_path + "': " + e.what());
    }

    if (!root["s2"]) {
        throw std::runtime_error("SceneWriter: YAML не содержит секцию 's2'");
    }

    // Строим YAML-узел из JSON-массива агентов
    YAML::Node agents_node = json_to_yaml(agents_json);
    root["s2"]["agents"] = agents_node;

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
