#pragma once
#include <s2/sim_engine.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <functional>

namespace s2 {

// HTTP REST сервер управления симуляцией.
// Команды → engine->enqueue(KernelCommand).
// GET запросы читают состояние из engine напрямую (eventual consistency).
class RestApiServer {
public:
    RestApiServer(int port, SimEngine* engine, std::string scenes_dir = "");
    ~RestApiServer();
    void start();
    void stop();
private:
    void run_server();
    void handle_client(int client_fd);

    struct HttpRequest { std::string method, path, body; };
    static HttpRequest parse_request(const std::string& raw);

    static void send_json(int fd, int status, const std::string& body);
    static void send_ok(int fd);
    static void send_error(int fd, int status, const std::string& msg);

    void dispatch(int fd, const HttpRequest& req);

    void handle_sim_pause(int fd);
    void handle_sim_resume(int fd);
    void handle_sim_reset(int fd);
    void handle_sim_step(int fd, const std::string& body);
    void handle_sim_speed(int fd, const std::string& body);
    void handle_sim_status(int fd);

    void handle_entities_list(int fd);
    void handle_entity_spawn(int fd, const std::string& body);
    void handle_entity_despawn(int fd, const std::string& path);
    void handle_entity_set_pose(int fd, const std::string& path, const std::string& body);
    void handle_entity_set_enabled(int fd, const std::string& path, const std::string& body);

    void handle_zones_list(int fd);
    void handle_zone_spawn(int fd, const std::string& body);
    void handle_zone_despawn(int fd, const std::string& path);
    void handle_zone_set_enabled(int fd, const std::string& path, const std::string& body);
    void handle_zone_set_strength(int fd, const std::string& path, const std::string& body);

    void handle_agent_plugin_input(int fd, const std::string& path, const std::string& body);

    void handle_scenes_list(int fd);
    void handle_scene_load(int fd, const std::string& body);

    int port_;
    SimEngine* engine_;
    std::string scenes_dir_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    int server_fd_{-1};
};

} // namespace s2
