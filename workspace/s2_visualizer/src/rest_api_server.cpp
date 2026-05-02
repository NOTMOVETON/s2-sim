#include "rest_api_server.hpp"
#include <s2/kernel_command.hpp>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <chrono>

namespace s2 {

RestApiServer::RestApiServer(int port, SimEngine* engine, std::string scenes_dir)
    : port_(port), engine_(engine), scenes_dir_(std::move(scenes_dir)) {}

RestApiServer::~RestApiServer() { stop(); }

void RestApiServer::start() {
    if (running_.load()) return;
    running_.store(true);
    thread_ = std::thread([this]() { run_server(); });
}

void RestApiServer::stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    if (thread_.joinable()) thread_.join();
}

void RestApiServer::run_server() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[RestApiServer] Failed to create socket" << std::endl;
        return;
    }
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[RestApiServer] Failed to bind port " << port_ << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return;
    }
    if (listen(server_fd_, 128) < 0) {
        std::cerr << "[RestApiServer] Failed to listen" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    int flags = fcntl(server_fd_, F_GETFL, 0);
    fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);

    std::cout << "[RestApiServer] Listening on port " << port_ << std::endl;

    while (running_.load()) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        std::thread([this, client_fd]() { handle_client(client_fd); }).detach();
    }

    close(server_fd_);
    server_fd_ = -1;
}

void RestApiServer::handle_client(int client_fd) {
    char buf[65536];
    ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return;
    }
    buf[n] = '\0';
    std::string raw(buf, n);

    auto req = parse_request(raw);

    // Read remaining body if Content-Length says there's more than what arrived
    auto find_header = [&](const char* hdr) {
        auto p = raw.find(hdr);
        if (p == std::string::npos) {
            std::string lower_hdr(hdr);
            std::transform(lower_hdr.begin(), lower_hdr.end(), lower_hdr.begin(), ::tolower);
            p = raw.find(lower_hdr);
        }
        return p;
    };
    auto cl_pos = find_header("Content-Length:");
    if (cl_pos != std::string::npos) {
        auto val_start = raw.find_first_not_of(" \t", cl_pos + 15);
        auto val_end = raw.find("\r\n", val_start);
        if (val_start != std::string::npos && val_end != std::string::npos) {
            int content_length = 0;
            try { content_length = std::stoi(raw.substr(val_start, val_end - val_start)); } catch(...) {}
            while ((int)req.body.size() < content_length) {
                char tmp[4096];
                int to_read = std::min(4096, content_length - (int)req.body.size());
                ssize_t nr = recv(client_fd, tmp, to_read, 0);
                if (nr <= 0) break;
                req.body.append(tmp, nr);
            }
        }
    }

    dispatch(client_fd, req);
}

RestApiServer::HttpRequest RestApiServer::parse_request(const std::string& raw) {
    HttpRequest req;
    auto eol = raw.find("\r\n");
    if (eol != std::string::npos) {
        std::string first_line = raw.substr(0, eol);
        auto sp1 = first_line.find(' ');
        if (sp1 != std::string::npos) {
            req.method = first_line.substr(0, sp1);
            auto sp2 = first_line.find(' ', sp1 + 1);
            if (sp2 != std::string::npos)
                req.path = first_line.substr(sp1 + 1, sp2 - sp1 - 1);
        }
    }
    auto sep = raw.find("\r\n\r\n");
    if (sep != std::string::npos)
        req.body = raw.substr(sep + 4);
    return req;
}

void RestApiServer::send_json(int fd, int status, const std::string& body) {
    std::string status_str;
    switch (status) {
        case 200: status_str = "200 OK"; break;
        case 400: status_str = "400 Bad Request"; break;
        case 404: status_str = "404 Not Found"; break;
        default:  status_str = std::to_string(status) + " Error"; break;
    }
    std::string response =
        "HTTP/1.1 " + status_str + "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n\r\n" + body;
    ::send(fd, response.c_str(), response.size(), MSG_NOSIGNAL);
    close(fd);
}

void RestApiServer::send_ok(int fd) {
    send_json(fd, 200, "{\"ok\":true}");
}

void RestApiServer::send_error(int fd, int status, const std::string& msg) {
    nlohmann::json j;
    j["error"] = msg;
    send_json(fd, status, j.dump());
}

// Extracts numeric entity ID from paths like /world/entities/{id} or /world/entities/{id}/something
static uint32_t extract_entity_id(const std::string& path, const std::string& prefix) {
    if (path.size() <= prefix.size()) return 0;
    std::string rest = path.substr(prefix.size());
    auto slash = rest.find('/');
    std::string id_str = (slash == std::string::npos) ? rest : rest.substr(0, slash);
    try { return static_cast<uint32_t>(std::stoul(id_str)); } catch(...) { return 0; }
}

// Extracts string zone ID from paths like /world/zones/{id} or /world/zones/{id}/something
static std::string extract_zone_id(const std::string& path) {
    const std::string prefix = "/world/zones/";
    if (path.size() <= prefix.size()) return "";
    std::string rest = path.substr(prefix.size());
    auto slash = rest.find('/');
    return (slash == std::string::npos) ? rest : rest.substr(0, slash);
}

void RestApiServer::dispatch(int fd, const HttpRequest& req) {
    const std::string& m = req.method;
    std::string path = req.path;
    auto qp = path.find('?');
    if (qp != std::string::npos) path = path.substr(0, qp);

    // CORS preflight
    if (m == "OPTIONS") {
        std::string resp =
            "HTTP/1.1 200 OK\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        ::send(fd, resp.c_str(), resp.size(), MSG_NOSIGNAL);
        close(fd);
        return;
    }

    // Sim control
    if (m == "POST" && path == "/sim/pause")  { handle_sim_pause(fd); return; }
    if (m == "POST" && path == "/sim/resume") { handle_sim_resume(fd); return; }
    if (m == "POST" && path == "/sim/reset")  { handle_sim_reset(fd); return; }
    if (m == "POST" && path == "/sim/step")   { handle_sim_step(fd, req.body); return; }
    if (m == "POST" && path == "/sim/speed")  { handle_sim_speed(fd, req.body); return; }
    if (m == "GET"  && path == "/sim/status") { handle_sim_status(fd); return; }

    // World entities
    if (m == "GET"    && path == "/world/entities")                                                     { handle_entities_list(fd); return; }
    if (m == "POST"   && path == "/world/entities")                                                     { handle_entity_spawn(fd, req.body); return; }
    if (m == "DELETE" && path.find("/world/entities/") == 0)                                            { handle_entity_despawn(fd, path); return; }
    if (m == "PUT"    && path.find("/world/entities/") == 0 && path.find("/pose") != std::string::npos) { handle_entity_set_pose(fd, path, req.body); return; }
    if (m == "PUT"    && path.find("/world/entities/") == 0 && path.find("/enabled") != std::string::npos) { handle_entity_set_enabled(fd, path, req.body); return; }

    // World zones
    if (m == "GET"    && path == "/world/zones")                                                           { handle_zones_list(fd); return; }
    if (m == "POST"   && path == "/world/zones")                                                           { handle_zone_spawn(fd, req.body); return; }
    if (m == "DELETE" && path.find("/world/zones/") == 0)                                                  { handle_zone_despawn(fd, path); return; }
    if (m == "PUT"    && path.find("/world/zones/") == 0 && path.find("/enabled") != std::string::npos)   { handle_zone_set_enabled(fd, path, req.body); return; }
    if (m == "PUT"    && path.find("/world/zones/") == 0 && path.find("/strength") != std::string::npos)  { handle_zone_set_strength(fd, path, req.body); return; }

    // Agents
    if (m == "POST" && path.find("/agents/") == 0 && path.find("/input/") != std::string::npos) {
        handle_agent_plugin_input(fd, path, req.body);
        return;
    }

    // Scenes
    if (m == "GET"  && path == "/scenes") { handle_scenes_list(fd); return; }
    if (m == "POST" && path == "/scenes") { handle_scene_load(fd, req.body); return; }

    send_error(fd, 404, "not found");
}

void RestApiServer::handle_sim_pause(int fd) {
    engine_->enqueue(cmd::PauseSim{});
    send_ok(fd);
}

void RestApiServer::handle_sim_resume(int fd) {
    engine_->enqueue(cmd::ResumeSim{});
    send_ok(fd);
}

void RestApiServer::handle_sim_reset(int fd) {
    engine_->enqueue(cmd::ResetSim{});
    send_ok(fd);
}

void RestApiServer::handle_sim_step(int fd, const std::string& body) {
    int n = 1;
    try { auto j = nlohmann::json::parse(body); n = j.value("n", 1); } catch(...) {}
    engine_->enqueue(cmd::StepSim{n});
    send_ok(fd);
}

void RestApiServer::handle_sim_speed(int fd, const std::string& body) {
    double factor = 1.0;
    try { auto j = nlohmann::json::parse(body); factor = j.value("factor", 1.0); } catch(...) {}
    engine_->enqueue(cmd::SetSpeed{factor});
    send_ok(fd);
}

void RestApiServer::handle_sim_status(int fd) {
    nlohmann::json j;
    j["paused"] = engine_->is_paused();
    j["sim_time"] = engine_->sim_time();
    send_json(fd, 200, j.dump());
}

void RestApiServer::handle_entities_list(int fd) {
    nlohmann::json j;
    j["agents"] = nlohmann::json::array();
    j["props"]  = nlohmann::json::array();
    j["actors"] = nlohmann::json::array();
    for (const auto& a  : engine_->world().agents()) j["agents"].push_back({{"id", a.id},  {"name", a.name}});
    for (const auto& p  : engine_->world().props())  j["props"].push_back({{"id", p.id},   {"type", p.type}});
    for (const auto& ac : engine_->world().actors())  j["actors"].push_back({{"id", ac.id}, {"name", ac.name}});
    send_json(fd, 200, j.dump());
}

void RestApiServer::handle_entity_spawn(int fd, const std::string& body) {
    try {
        auto j = nlohmann::json::parse(body);
        cmd::SpawnEntity c;
        c.id = j.value("id", uint32_t(0));
        std::string type_str = j.value("type", "agent");
        if (type_str == "actor")     c.type = EntityType::ACTOR;
        else if (type_str == "prop") c.type = EntityType::PROP;
        else                         c.type = EntityType::AGENT;
        c.name = j.value("name", "");
        if (j.contains("pose")) {
            c.pose.x   = j["pose"].value("x", 0.0);
            c.pose.y   = j["pose"].value("y", 0.0);
            c.pose.z   = j["pose"].value("z", 0.0);
            c.pose.yaw = j["pose"].value("yaw", 0.0);
        }
        engine_->enqueue(std::move(c));
        send_ok(fd);
    } catch(const std::exception& e) { send_error(fd, 400, e.what()); }
}

void RestApiServer::handle_entity_despawn(int fd, const std::string& path) {
    EntityId id = extract_entity_id(path, "/world/entities/");
    engine_->enqueue(cmd::DespawnEntity{id});
    send_ok(fd);
}

void RestApiServer::handle_entity_set_pose(int fd, const std::string& path, const std::string& body) {
    EntityId id = extract_entity_id(path, "/world/entities/");
    try {
        auto j = nlohmann::json::parse(body);
        Pose3D pose{};
        pose.x   = j.value("x",   0.0);
        pose.y   = j.value("y",   0.0);
        pose.z   = j.value("z",   0.0);
        pose.yaw = j.value("yaw", 0.0);
        engine_->enqueue(cmd::SetPose{id, pose});
        send_ok(fd);
    } catch(const std::exception& e) { send_error(fd, 400, e.what()); }
}

void RestApiServer::handle_entity_set_enabled(int fd, const std::string& path, const std::string& body) {
    EntityId id = extract_entity_id(path, "/world/entities/");
    try {
        auto j = nlohmann::json::parse(body);
        engine_->enqueue(cmd::SetEnabled{id, j.value("enabled", true)});
        send_ok(fd);
    } catch(const std::exception& e) { send_error(fd, 400, e.what()); }
}

void RestApiServer::handle_zones_list(int fd) {
    nlohmann::json j;
    j["zones"] = nlohmann::json::array();
    for (const auto& z : engine_->zone_system().all_zones())
        j["zones"].push_back({{"id", z.id}, {"enabled", z.enabled}});
    send_json(fd, 200, j.dump());
}

void RestApiServer::handle_zone_spawn(int fd, const std::string& body) {
    try {
        auto j = nlohmann::json::parse(body);
        cmd::SpawnZone c;
        c.id      = j.value("id", std::string(""));
        c.enabled = j.value("enabled", true);
        if (j.contains("shape")) {
            auto& s = j["shape"];
            std::string st = s.value("type", "sphere");
            if (st == "aabb")         c.shape.type = ZoneShapeType::AABB;
            else if (st == "cylinder") c.shape.type = ZoneShapeType::CYLINDER;
            else if (st == "infinite") c.shape.type = ZoneShapeType::INFINITE;
            else                       c.shape.type = ZoneShapeType::SPHERE;
            c.shape.radius      = s.value("radius", 1.0);
            c.shape.half_height = s.value("half_height", 1.0);
            if (s.contains("center") && s["center"].is_array() && s["center"].size() >= 3)
                c.shape.center = Vec3(s["center"][0].get<double>(), s["center"][1].get<double>(), s["center"][2].get<double>());
            if (s.contains("half_size") && s["half_size"].is_array() && s["half_size"].size() >= 3)
                c.shape.half_size = Vec3(s["half_size"][0].get<double>(), s["half_size"][1].get<double>(), s["half_size"][2].get<double>());
        }
        engine_->enqueue(std::move(c));
        send_ok(fd);
    } catch(const std::exception& e) { send_error(fd, 400, e.what()); }
}

void RestApiServer::handle_zone_despawn(int fd, const std::string& path) {
    engine_->enqueue(cmd::DespawnZone{extract_zone_id(path)});
    send_ok(fd);
}

void RestApiServer::handle_zone_set_enabled(int fd, const std::string& path, const std::string& body) {
    try {
        auto j = nlohmann::json::parse(body);
        engine_->enqueue(cmd::ToggleZone{extract_zone_id(path), j.value("enabled", true)});
        send_ok(fd);
    } catch(const std::exception& e) { send_error(fd, 400, e.what()); }
}

void RestApiServer::handle_zone_set_strength(int fd, const std::string& path, const std::string& body) {
    try {
        auto j = nlohmann::json::parse(body);
        engine_->enqueue(cmd::SetZoneStrength{extract_zone_id(path), j.value("strength", 1.0)});
        send_ok(fd);
    } catch(const std::exception& e) { send_error(fd, 400, e.what()); }
}

void RestApiServer::handle_agent_plugin_input(int fd, const std::string& path, const std::string& body) {
    // path: /agents/{id}/input/{plugin}
    const std::string prefix = "/agents/";
    if (path.size() <= prefix.size()) { send_error(fd, 400, "invalid path"); return; }
    std::string rest = path.substr(prefix.size());
    auto slash1 = rest.find('/');
    if (slash1 == std::string::npos) { send_error(fd, 400, "invalid path"); return; }
    std::string id_str = rest.substr(0, slash1);
    std::string remain = rest.substr(slash1 + 1); // "input/{plugin}"
    auto slash2 = remain.find('/');
    if (slash2 == std::string::npos) { send_error(fd, 400, "invalid path"); return; }
    std::string plugin = remain.substr(slash2 + 1);
    AgentId agent_id = 0;
    try { agent_id = static_cast<AgentId>(std::stoul(id_str)); } catch(...) {}
    engine_->handle_plugin_input(agent_id, plugin, body);
    send_ok(fd);
}

void RestApiServer::handle_scenes_list(int fd) {
    nlohmann::json j;
    j["scenes"] = nlohmann::json::array();
    if (!scenes_dir_.empty()) {
        try {
            std::vector<std::string> names;
            for (const auto& entry : std::filesystem::directory_iterator(scenes_dir_))
                if (entry.path().extension() == ".yaml")
                    names.push_back(entry.path().filename().string());
            std::sort(names.begin(), names.end());
            for (const auto& n : names) j["scenes"].push_back(n);
        } catch(...) {}
    }
    send_json(fd, 200, j.dump());
}

void RestApiServer::handle_scene_load(int fd, const std::string& body) {
    try {
        auto j = nlohmann::json::parse(body);
        std::string filename = j.value("filename", "");
        if (filename.empty() || filename.find('/') != std::string::npos || filename.find("..") != std::string::npos) {
            send_error(fd, 400, "invalid filename");
            return;
        }
        engine_->enqueue(cmd::LoadScene{scenes_dir_ + "/" + filename});
        send_ok(fd);
    } catch(const std::exception& e) { send_error(fd, 400, e.what()); }
}

} // namespace s2
