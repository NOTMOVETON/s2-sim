#include <s2/scene_loader.hpp>
#include <s2/scene_writer.hpp>
#include <s2/sim_engine.hpp>
#include <s2/effects_registry.hpp>
#include <s2/plugins/plugin_base.hpp>
#include <s2/plugins/diff_drive.hpp>
#include <s2/plugins/gnss.hpp>
#include <s2/plugins/imu.hpp>
#include <s2/ros2_transport_adapter.hpp>
#include <s2/sim_transport_bridge.hpp>
#include "viz_server.hpp"

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <csignal>
#include <string>
#include <memory>

namespace {

// Рекурсивное преобразование YAML::Node → nlohmann::json
nlohmann::json yaml_to_json(const YAML::Node& node)
{
    switch (node.Type())
    {
        case YAML::NodeType::Null:
            return nullptr;

        case YAML::NodeType::Scalar:
        {
            // Пробуем числа и булевы
            try { return node.as<int64_t>(); } catch (...) {}
            try { return node.as<double>(); } catch (...) {}
            try { return node.as<bool>(); } catch (...) {}
            return node.as<std::string>();
        }

        case YAML::NodeType::Sequence:
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& item : node)
                arr.push_back(yaml_to_json(item));
            return arr;
        }

        case YAML::NodeType::Map:
        {
            nlohmann::json obj = nlohmann::json::object();
            for (const auto& kv : node)
                obj[kv.first.as<std::string>()] = yaml_to_json(kv.second);
            return obj;
        }

        default:
            return nullptr;
    }
}

} // anonymous namespace

#ifdef S2_WITH_ROS2
#include <rclcpp/rclcpp.hpp>
#endif

namespace {

s2::SimEngine*         g_engine = nullptr;
s2::VizServer*         g_viz    = nullptr;
s2::SimTransportBridge* g_bridge = nullptr;

// Мгновенно отправить обновлённый снапшот всем SSE-клиентам
static void broadcast_snapshot() {
    if (g_engine && g_viz) {
        g_viz->publish(g_engine->build_snapshot());
        g_viz->force_broadcast_latest();
    }
}

void signal_handler(int signum) {
    (void)signum;
    std::cout << "\n[Main] Shutting down..." << std::endl;
    if (g_engine) g_engine->stop();
    if (g_viz)    g_viz->stop();
    if (g_bridge) g_bridge->stop();
}

/**
 * @brief Адаптер, связывающий VizCommandHandler с SimEngine.
 * Превращает команды от визуализатора в вызовы методов SimEngine.
 */
class SimEngineCommandAdapter : public s2::VizCommandHandler {
public:
    explicit SimEngineCommandAdapter(s2::SimEngine* engine, s2::VizServer* viz,
                                     std::string scene_path = "",
                                     s2::SceneLoader::PluginFactory plugin_factory = {})
        : engine_(engine), viz_(viz),
          scene_path_(std::move(scene_path)),
          plugin_factory_(std::move(plugin_factory))
    {
        if (!scene_path_.empty()) {
            scenes_dir_ = std::filesystem::path(scene_path_).parent_path().string();
        }
    }

    void on_pause() override {
        if (engine_) engine_->pause();
        broadcast_snapshot();
    }

    void on_resume() override {
        if (engine_) engine_->resume();
        broadcast_snapshot();
    }

    void on_reset() override {
        if (engine_) engine_->reset();
        broadcast_snapshot();
    }

    void on_move_agent(s2::AgentId id, double x, double y, double yaw) override {
        if (engine_) {
            s2::Pose3D pose{};
            pose.x = x;
            pose.y = y;
            pose.yaw = yaw;
            engine_->set_agent_pose(id, pose);
            broadcast_snapshot();
        }
    }

    void on_plugin_input(s2::AgentId agent_id, const std::string& plugin_type,
                         const std::string& json_input) override {
        if (engine_) {
            engine_->handle_plugin_input(agent_id, plugin_type, json_input);
            broadcast_snapshot();
        }
    }

    void on_update_geometry(const std::vector<s2::WorldPrimitive>& prims) override {
        if (!engine_) return;
        // Обновляем геометрию в SimWorld И синхронизируем систему коллизий
        engine_->update_static_geometry(prims);
        if (viz_) {
            viz_->publish(engine_->build_snapshot());
            viz_->force_broadcast_with_geometry();
        }
    }

    SaveSceneResult on_save_scene() override {
        if (scene_path_.empty()) {
            return {false, "путь к файлу сцены не задан"};
        }
        if (!engine_) {
            return {false, "движок не инициализирован"};
        }
        try {
            s2::SceneWriter::save_geometry(scene_path_, engine_->world().static_geometry());
            return {true, scene_path_};
        } catch (const std::exception& e) {
            return {false, e.what()};
        }
    }

    std::string on_get_scene_state() override {
        if (scene_path_.empty()) {
            return "{\"agents\":[],\"geometry\":[]}";
        }
        try {
            YAML::Node root = YAML::LoadFile(scene_path_);
            nlohmann::json j;
            j["yaml_path"] = scene_path_;
            j["agents"] = nlohmann::json::array();

            if (root["s2"] && root["s2"]["agents"]) {
                for (const auto& agent_node : root["s2"]["agents"]) {
                    nlohmann::json agent = yaml_to_json(agent_node);
                    j["agents"].push_back(agent);
                }
            }

            j["geometry"] = nlohmann::json::array();
            if (root["s2"] && root["s2"]["world"] && root["s2"]["world"]["geometry"]) {
                j["geometry"] = yaml_to_json(root["s2"]["world"]["geometry"]);
            }

            return j.dump();
        } catch (const std::exception& e) {
            return std::string("{\"error\":\"") + e.what() + "\"}";
        }
    }

    std::string on_get_urdf_list() override {
        nlohmann::json j;
        j["files"] = nlohmann::json::array();

        if (scene_path_.empty()) return j.dump();

        try {
            // Ищем URDF в <scene_dir>/../robots/
            std::filesystem::path scene_dir =
                std::filesystem::path(scene_path_).parent_path();
            std::filesystem::path robots_dir = scene_dir / ".." / "robots";

            if (std::filesystem::exists(robots_dir)) {
                for (const auto& entry :
                     std::filesystem::directory_iterator(robots_dir))
                {
                    if (entry.path().extension() == ".urdf") {
                        // Возвращаем путь относительно директории сцены
                        std::string rel = std::filesystem::relative(
                            entry.path(), scene_dir).string();
                        j["files"].push_back(rel);
                    }
                }
            }
        } catch (...) {}

        return j.dump();
    }

    SaveSceneResult on_update_agents(const std::string& agents_json) override {
        if (scene_path_.empty()) {
            return {false, "путь к файлу сцены не задан"};
        }
        try {
            auto j = nlohmann::json::parse(agents_json);
            // Принимаем как массив напрямую или как {"agents":[...]}
            nlohmann::json agents_array = j.is_array() ? j : j.value("agents", nlohmann::json::array());
            s2::SceneWriter::save_agents(scene_path_, agents_array);
            return {true, scene_path_};
        } catch (const std::exception& e) {
            return {false, e.what()};
        }
    }

    std::string on_get_scene_list() override {
        nlohmann::json j;
        j["scenes"] = nlohmann::json::array();
        if (scenes_dir_.empty()) return j.dump();
        try {
            std::vector<std::string> names;
            for (const auto& entry :
                 std::filesystem::directory_iterator(scenes_dir_))
            {
                if (entry.path().extension() == ".yaml") {
                    names.push_back(entry.path().filename().string());
                }
            }
            std::sort(names.begin(), names.end());
            for (const auto& n : names) j["scenes"].push_back(n);
        } catch (...) {}
        return j.dump();
    }

    SaveSceneResult on_load_scene(const std::string& filename) override {
        if (scenes_dir_.empty())
            return {false, "директория сцен не задана"};
        if (!engine_)
            return {false, "движок не инициализирован"};
        // Защита от path traversal
        if (filename.find('/') != std::string::npos ||
            filename.find("..") != std::string::npos)
            return {false, "недопустимое имя файла"};

        std::string full_path = scenes_dir_ + "/" + filename;
        if (!std::filesystem::exists(full_path))
            return {false, "файл не найден: " + filename};

        try {
            engine_->pause();
            auto new_data = s2::SceneLoader::load(full_path, plugin_factory_);

            s2::SimWorld new_world;
            new_world.set_heightmap(std::move(new_data.heightmap));
            for (auto& g  : new_data.geometry) new_world.add_static_primitive(std::move(g));
            for (auto& a  : new_data.agents)   new_world.add_agent(std::move(a));
            for (auto& p  : new_data.props)     new_world.add_prop(std::move(p));
            for (auto& ac : new_data.actors)    new_world.add_actor(std::move(ac));
            for (auto& z  : new_data.zones)     new_world.add_zone(std::move(z));

            engine_->load_world(std::move(new_world));
            scene_path_ = full_path;
            scenes_dir_ = std::filesystem::path(scene_path_).parent_path().string();

            if (viz_) viz_->force_broadcast_with_geometry();
            engine_->resume();
            return {true, full_path};
        } catch (const std::exception& e) {
            engine_->resume();
            return {false, e.what()};
        }
    }

    SaveSceneResult on_save_scene_as(const std::string& new_name) override {
        if (scene_path_.empty() || scenes_dir_.empty())
            return {false, "сцена не загружена"};
        if (new_name.empty() ||
            new_name.find('/') != std::string::npos ||
            new_name.find("..") != std::string::npos)
            return {false, "недопустимое имя файла"};

        std::string fname = new_name;
        if (fname.size() < 5 || fname.substr(fname.size() - 5) != ".yaml")
            fname += ".yaml";

        std::string dest = scenes_dir_ + "/" + fname;
        try {
            std::filesystem::copy_file(
                scene_path_, dest,
                std::filesystem::copy_options::overwrite_existing);
            // Перезаписать geometry актуальными примитивами
            if (engine_)
                s2::SceneWriter::save_geometry(dest, engine_->world().static_geometry());
            return {true, dest};
        } catch (const std::exception& e) {
            return {false, e.what()};
        }
    }

    SaveSceneResult on_new_scene(const std::string& new_name) override {
        if (scenes_dir_.empty())
            return {false, "директория сцен не задана"};
        if (new_name.empty() ||
            new_name.find('/') != std::string::npos ||
            new_name.find("..") != std::string::npos)
            return {false, "недопустимое имя"};

        std::string fname = new_name;
        if (fname.size() < 5 || fname.substr(fname.size() - 5) != ".yaml")
            fname += ".yaml";

        std::string dest = scenes_dir_ + "/" + fname;
        if (std::filesystem::exists(dest))
            return {false, "файл уже существует: " + fname};

        // Минимальная сцена с одним агентом
        static const char* EMPTY_SCENE =
            "s2:\n"
            "  update_rate: 50\n"
            "  visualizer:\n"
            "    enabled: true\n"
            "    port: 1937\n"
            "  transport:\n"
            "    type: stub\n"
            "  world:\n"
            "    geometry: []\n"
            "  agents:\n"
            "    - name: robot_0\n"
            "      pose: { x: 0.0, y: 0.0 }\n"
            "      visual:\n"
            "        type: box\n"
            "        size: [0.6, 0.4, 0.3]\n"
            "        color: \"#FF6B35\"\n"
            "      plugins:\n"
            "        - type: diff_drive\n"
            "          wheel_base: 0.4\n"
            "          max_linear_vel: 1.5\n"
            "          max_angular_vel: 2.0\n";
        try {
            std::ofstream f(dest);
            if (!f.is_open()) return {false, "не удалось создать файл"};
            f << EMPTY_SCENE;
            if (f.fail()) return {false, "ошибка записи"};
        } catch (const std::exception& e) {
            return {false, e.what()};
        }
        return on_load_scene(fname);
    }

private:
    s2::SimEngine* engine_;
    s2::VizServer* viz_;
    std::string    scene_path_;
    std::string    scenes_dir_;
    s2::SceneLoader::PluginFactory plugin_factory_;
};

} // anonymous namespace

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

#ifdef S2_WITH_ROS2
    // Инициализируем глобальный ROS2 контекст (domain 0).
    // Изолированные контексты (domain 50/51/52) инициализируются отдельно в адаптере,
    // но глобальная инициализация нужна для корректной работы DDS.
    rclcpp::init(argc, argv);
#endif

    // Путь к сцене
    std::string scene_path = "/workspace/s2_config/scenes/test_basic.yaml";
    if (argc > 1) {
        scene_path = argv[1];
    }

    std::cout << "[Main] Loading scene: " << scene_path << std::endl;

    s2::SceneLoader::PluginFactory plugin_factory = s2::plugins::create_plugin;

    s2::SceneData scene_data;
    try {
        scene_data = s2::SceneLoader::load(scene_path, plugin_factory);
    } catch (const YAML::Exception& e) {
        std::cerr << "[Main] YAML error at line " << e.mark.line << ": " << e.msg << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[Main] Failed to load scene: " << e.what() << std::endl;
        return 1;
    }

    // Создаём визуализатор
    std::string web_path = "/workspace/s2_visualizer/web";

    std::unique_ptr<s2::VizServer> viz;
    if (scene_data.viz_config.enabled) {
        int port = scene_data.viz_config.port;
        std::cout << "[Main] Starting VizServer on port " << port << "..." << std::endl;
        viz = std::make_unique<s2::VizServer>(0, port, web_path);
        viz->start();
        g_viz = viz.get();
    } else {
        std::cout << "[Main] Visualizer disabled (headless mode)" << std::endl;
    }

    // Собираем мир из сцены
    s2::SimWorld world;
    world.set_heightmap(std::move(scene_data.heightmap));

    for (auto& geom : scene_data.geometry)   world.add_static_primitive(std::move(geom));
    for (auto& agent : scene_data.agents)    world.add_agent(std::move(agent));
    for (auto& prop : scene_data.props)      world.add_prop(std::move(prop));
    for (auto& actor : scene_data.actors)    world.add_actor(std::move(actor));
    for (auto& zone : scene_data.zones)      world.add_zone(std::move(zone));

    // Создаём движок
    s2::SimEngine engine(scene_data.engine_config);
    engine.set_effect_factory(s2::create_effect);
    engine.load_world(std::move(world));
    engine.set_viz_server(g_viz);
    g_engine = &engine;

    // Подключаем обработчик команд от визуализатора (передаём путь к YAML для сохранения)
    SimEngineCommandAdapter cmd_adapter(&engine, g_viz, scene_path, plugin_factory);
    if (g_viz) g_viz->set_command_handler(&cmd_adapter);

    // Настраиваем GNSS плагины: устанавливаем geo_origin
    if (scene_data.geo_origin.is_set()) {
        for (auto& agent : engine.world().agents()) {
            for (auto& plugin : agent.plugins) {
                if (plugin->type() == "gnss") {
                    auto* gnss = dynamic_cast<s2::plugins::GnssPlugin*>(plugin.get());
                    if (gnss) {
                        gnss->set_geo_origin(scene_data.geo_origin);
                        std::cout << "[Main] GNSS geo_origin set for agent: "
                                  << agent.name << std::endl;
                    }
                }
            }
        }
    }

    // Создаём транспортный адаптер и мост
    // bridge->init() регистрирует агентов, сенсоры, топики и устанавливает post-tick callback
    std::shared_ptr<s2::ITransportAdapter> adapter;
    if (scene_data.transport_config.type == "stub") {
        adapter = std::make_shared<s2::Ros2TransportAdapter>();
        std::cout << "[Main] Transport: stub mode" << std::endl;
    } else {
#ifndef S2_WITH_ROS2
        std::cerr << "[Main] WARNING: transport type=ros2 but S2_WITH_ROS2 not set, falling back to stub" << std::endl;
#endif
        adapter = std::make_shared<s2::Ros2TransportAdapter>();
        std::cout << "[Main] Transport: ros2 (domain default="
                  << scene_data.transport_config.default_domain_id << ")" << std::endl;
    }
    auto bridge  = std::make_unique<s2::SimTransportBridge>(&engine, adapter);
    bridge->init(scene_data.geo_origin);
    bridge->start();
    g_bridge = bridge.get();

    // Отправляем начальный снапшот
    if (viz) viz->publish(engine.build_snapshot());

    std::cout << "[Main] Scene loaded. Agents: " << engine.world().agents().size()
              << ", Props: " << engine.world().props().size()
              << ", Actors: " << engine.world().actors().size()
              << std::endl;

    std::cout << "[Main] Starting simulation..." << std::endl;
    engine.run();

    std::cout << "[Main] Simulation stopped. Exiting." << std::endl;
    if (viz) viz->stop();

#ifdef S2_WITH_ROS2
    rclcpp::shutdown();
#endif

    return 0;
}
