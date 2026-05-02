#include <s2/scene_loader.hpp>
#include <s2/sim_engine.hpp>
#include <s2/effects_registry.hpp>
#include <s2/plugins/plugin_base.hpp>
#include <s2/plugins/diff_drive.hpp>
#include <s2/plugins/gnss.hpp>
#include <s2/plugins/imu.hpp>
#include <s2/ros2_transport_adapter.hpp>
#include <s2/sim_transport_bridge.hpp>
#include "web_viz_adapter.hpp"
#include "rest_api_server.hpp"

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <iostream>
#include <csignal>
#include <string>
#include <memory>

#ifdef S2_WITH_ROS2
#include <rclcpp/rclcpp.hpp>
#endif

namespace {

s2::SimEngine*         g_engine = nullptr;
s2::WebVizAdapter*     g_viz    = nullptr;
s2::SimTransportBridge* g_bridge = nullptr;
s2::RestApiServer*     g_rest   = nullptr;

void signal_handler(int signum) {
    (void)signum;
    std::cout << "\n[Main] Shutting down..." << std::endl;
    if (g_engine) g_engine->stop();
    if (g_viz)    g_viz->stop();
    if (g_bridge) g_bridge->stop();
    if (g_rest)   g_rest->stop();
}

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

    std::unique_ptr<s2::WebVizAdapter> viz;
    if (scene_data.viz_config.enabled) {
        int port = scene_data.viz_config.port;
        std::cout << "[Main] Starting VizServer on port " << port << "..." << std::endl;
        viz = std::make_unique<s2::WebVizAdapter>(port, web_path);
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
    engine.set_viz_adapter(g_viz);
    g_engine = &engine;

    // Запускаем REST API сервер на порту viz+1
    int rest_port = scene_data.viz_config.port + 1;
    std::string scenes_dir = std::filesystem::path(scene_path).parent_path().string();
    auto rest_server = std::make_unique<s2::RestApiServer>(rest_port, &engine, scenes_dir);
    rest_server->start();
    g_rest = rest_server.get();
    std::cout << "[Main] REST API on port " << rest_port << std::endl;

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
    rest_server->stop();
    if (viz) viz->stop();

#ifdef S2_WITH_ROS2
    rclcpp::shutdown();
#endif

    return 0;
}
