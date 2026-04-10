#include "viz_server.hpp"
#include <nlohmann/json.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <thread>
#include <arpa/inet.h>

namespace s2 {

static VizServer* g_viz_server = nullptr;

// Minimal SHA-1 implementation for WebSocket handshake
static void sha1(const uint8_t* data, size_t len, uint8_t hash[20]) {
    // Simplified SHA-1 - actually we can use the openssl one
    // For now use a simple approach
    std::memset(hash, 0, 20);
}

// Manual SHA-1 + Base64 for WebSocket
static std::string compute_ws_accept(const std::string& key) {
    // Use openssl if available, fallback to calling sha1sum
    std::string cmd = "echo -n '" + key + "258EAFA5-E914-47DA-95CA-5AB37E8E73AB' | openssl sha1 -binary | openssl base64";
    char buffer[128];
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        pclose(pipe);
    }
    // Trim newline
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

VizServer::VizServer(int ws_port, int http_port, const std::string& static_path)
    : ws_port_(ws_port), http_port_(http_port), static_path_(static_path),
      server_fd_(-1)
{
}

VizServer::~VizServer() {
    stop();
}

// Статическая обёртка для запуска SSE в отдельном потоке
static void sse_thread_func(VizServer* server, int client_fd) {
    server->run_sse_client(client_fd);
}

void VizServer::start() {
    if (running_.load()) return;
    g_viz_server = this;
    running_.store(true);
    thread_ = std::thread([this]() { run_server(); });
}

void VizServer::stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    g_viz_server = nullptr;
}

void VizServer::publish(const WorldSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    pending_snapshot_ = snapshot;
    has_pending_.store(true);
}

// Мгновенная отправка снапшота всем SSE-клиентам (вызывается после команд)
void VizServer::force_broadcast(const WorldSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    nlohmann::json j = snapshot_to_json(snapshot);
    std::string msg = "data: " + j.dump() + "\n\n";
    for (int fd : ws_clients_) {
        send(fd, msg.c_str(), msg.size(), MSG_NOSIGNAL);
    }
}

int VizServer::client_count() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return static_cast<int>(ws_clients_.size());
}

static std::string get_mime(const std::string& path) {
    if (path.size() > 5 && path.substr(path.size() - 5) == ".html") return "text/html";
    if (path.size() > 3 && path.substr(path.size() - 3) == ".js") return "application/javascript";
    if (path.size() > 4 && path.substr(path.size() - 4) == ".css") return "text/css";
    return "application/octet-stream";
}

static std::string read_file_content(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return "";
    auto sz = f.tellg();
    f.seekg(0);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

static std::vector<uint8_t> ws_text_frame(const std::string& data) {
    std::vector<uint8_t> frame;
    frame.push_back(0x81); // FIN + text opcode
    size_t len = data.size();
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len < 65536) {
        frame.push_back(126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    }
    frame.insert(frame.end(), data.begin(), data.end());
    return frame;
}

void VizServer::send_snapshot_now(int client_fd) {
    // Копируем снапшот под мьютексом, сериализуем снаружи
    WorldSnapshot snap_copy;
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        const WorldSnapshot* snap = nullptr;
        if (has_pending_.load() && !pending_snapshot_.agents.empty()) {
            snap = &pending_snapshot_;
        } else if (current_snapshot_) {
            snap = &(*current_snapshot_);
        }
        if (!snap) return;
        snap_copy = *snap;
    }
    nlohmann::json j = snapshot_to_json(snap_copy, true, true); // include geometry + plugins при подключении
    std::string sse_msg = "data: " + j.dump() + "\n\n";
    send(client_fd, sse_msg.c_str(), sse_msg.size(), MSG_NOSIGNAL);
}

void VizServer::handle_pending_snapshots() {
    if (!has_pending_.load()) return;

    // Шаг 1: копируем снапшот под мьютексом и сразу отпускаем его.
    // publish() из потока симуляции берёт тот же мьютекс — чем быстрее мы его отпустим,
    // тем меньше задержка симуляции.
    WorldSnapshot snap_copy;
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        if (!has_pending_.load()) return;
        if (pending_snapshot_.agents.empty() && pending_snapshot_.props.empty() && pending_snapshot_.actors.empty()) return;
        snap_copy = pending_snapshot_;
        current_snapshot_ = pending_snapshot_;
        has_pending_.store(false);
    }

    // Шаг 2: JSON-сериализация вне мьютексов (может быть медленной при большом количестве точек)
    bool include_plugins = (snap_counter_++ % PLUGIN_DATA_INTERVAL == 0);
    std::string sse_msg = "data: " + snapshot_to_json(snap_copy, false, include_plugins).dump() + "\n\n";

    // Шаг 3: рассылка клиентам
    std::lock_guard<std::mutex> cl(clients_mutex_);
    std::vector<int> dead_clients;
    for (int fd : ws_clients_) {
        ssize_t sent = send(fd, sse_msg.c_str(), sse_msg.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            dead_clients.push_back(fd);
        }
    }
    for (int fd : dead_clients) {
        close(fd);
        ws_clients_.erase(fd);
    }
}

void VizServer::handle_websocket_loop(int client_fd) {
    char buf[4096];
    auto last_send = std::chrono::steady_clock::now();
    
    while (running_.load()) {
        // Check for incoming WS frames (non-blocking since client_fd is non-blocking)
        ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data — send pending snapshots if needed
                handle_pending_snapshots();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            break; // Error
        }
        if (n <= 0) break;  // Client disconnected
        // Ignore received data (could be close/ping frames) — но всё равно шлём снапшоты
        handle_pending_snapshots();
    }
    
    std::lock_guard<std::mutex> lock(clients_mutex_);
    ws_clients_.erase(client_fd);
    close(client_fd);
}

bool VizServer::is_websocket_handshake(const std::string& request) {
    return request.find("Sec-WebSocket-Key:") != std::string::npos &&
           request.find("Upgrade: websocket") != std::string::npos;
}

void VizServer::handle_websocket_upgrade(int client_fd, const std::string& request) {
    size_t ws_key_pos = request.find("Sec-WebSocket-Key: ");
    size_t key_start = ws_key_pos + 19;
    size_t key_end = request.find("\r\n", key_start);
    std::string ws_key = request.substr(key_start, key_end - key_start);
    
    std::string accept_key = compute_ws_accept(ws_key);
    
    std::string handshake = 
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept_key + "\r\n"
        "\r\n";
    send(client_fd, handshake.c_str(), handshake.size(), 0);
    
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        ws_clients_.insert(client_fd);
    }
    std::cout << "[VizServer] WebSocket client connected, total: " << client_count() << std::endl;
    
    send_snapshot_now(client_fd);
    handle_websocket_loop(client_fd);
}

// Глобальная ссылка на сервер для доступа из handle_command
static VizServer* g_broadcast_server = nullptr;

void VizServer::force_broadcast_latest() {
    // Копируем снапшот под мьютексом, сразу отпускаем
    WorldSnapshot snap_copy;
    {
        std::lock_guard<std::mutex> lk(snapshot_mutex_);
        const WorldSnapshot* snap = nullptr;
        if (has_pending_.load()) snap = &pending_snapshot_;
        else if (current_snapshot_) snap = &*current_snapshot_;
        if (!snap) return;
        snap_copy = *snap;
    }
    // Сериализация без мьютекса; force_broadcast всегда включает plugins_data
    std::string msg = "data: " + snapshot_to_json(snap_copy, false, true).dump() + "\n\n";

    std::vector<int> clients_copy;
    {
        std::lock_guard<std::mutex> cl(clients_mutex_);
        clients_copy = std::vector<int>(ws_clients_.begin(), ws_clients_.end());
    }
    for (int fd : clients_copy) {
        send(fd, msg.c_str(), msg.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
    }
}

// URL decode helper
static std::string url_decode(const std::string& str) {
    std::string result;
    char ch;
    int i = 0;
    while (i < (int)str.size()) {
        if (str[i] == '%') {
            if (i + 2 < (int)str.size()) {
                std::string hex = str.substr(i + 1, 2);
                char* end;
                ch = (char)strtol(hex.c_str(), &end, 16);
                result += ch;
                i += 3;
                continue;
            }
        } else if (str[i] == '+') {
            result += ' ';
            i++;
            continue;
        }
        result += str[i];
        i++;
    }
    return result;
}

// Обработать POST/GET команду из визуализатора
static void handle_command(int client_fd, VizCommandHandler* handler, const std::string& full_url) {
    auto qp = full_url.find('?');
    std::map<std::string, std::string> params;
    if (qp != std::string::npos) {
        std::string query_str = full_url.substr(qp + 1);
        std::istringstream iss(query_str);
        std::string pair;
        while (std::getline(iss, pair, '&')) {
            auto eq = pair.find('=');
            if (eq != std::string::npos) {
                params[pair.substr(0, eq)] = pair.substr(eq + 1);
            }
        }
    }

    std::string cmd = params.count("cmd") ? params["cmd"] : "";
    bool cmd_handled = false;

    if (handler) {
        if (cmd == "pause") {
            handler->on_pause();
            cmd_handled = true;
        } else if (cmd == "resume") {
            handler->on_resume();
            cmd_handled = true;
        } else if (cmd == "reset") {
            handler->on_reset();
            cmd_handled = true;
        } else if (cmd == "reset_and_resume") {
            handler->on_reset();
            handler->on_resume();
            cmd_handled = true;
        } else if (cmd == "move_agent") {
            AgentId id = 0;
            double x = 0, y = 0, yaw = 0;
            if (params.count("id")) try { id = std::stoull(params["id"]); } catch (...) {}
            if (params.count("x")) try { x = std::stod(params["x"]); } catch (...) {}
            if (params.count("y")) try { y = std::stod(params["y"]); } catch (...) {}
            if (params.count("yaw")) try { yaw = std::stod(params["yaw"]); } catch (...) {}
            handler->on_move_agent(id, x, y, yaw);
            cmd_handled = true;
        } else if (cmd == "plugin_input") {
            AgentId agent_id = 0;
            std::string plugin_type;
            std::string json_input;
            if (params.count("agent_id")) try { agent_id = std::stoull(params["agent_id"]); } catch (...) {}
            if (params.count("plugin")) plugin_type = params["plugin"];
            if (params.count("body")) json_input = url_decode(params["body"]);
            
            if (!plugin_type.empty() && !json_input.empty()) {
                handler->on_plugin_input(agent_id, plugin_type, json_input);
                cmd_handled = true;
            }
        }

        if (cmd_handled && g_broadcast_server) {
            g_broadcast_server->force_broadcast_latest();
        }
    }

    nlohmann::json response = {{"status", "ok"}, {"cmd", cmd}};
    std::string body = response.dump();
    std::string http_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n\r\n" + body;
    send(client_fd, http_response.c_str(), http_response.size(), 0);
    close(client_fd);
}

void VizServer::serve_http(int client_fd, const std::string& request) {
    std::string url = "/";
    size_t sp = request.find(' ');
    if (sp != std::string::npos) {
        size_t sp2 = request.find(' ', sp + 1);
        if (sp2 != std::string::npos) {
            url = request.substr(sp + 1, sp2 - sp - 1);
        }
    }

    if (url.find("/command") != std::string::npos) {
        g_broadcast_server = this;
        handle_command(client_fd, command_handler_, url);
        return;
    }
    
    if (url.empty() || url == "/") url = "/index.html";
    auto qp = url.find('?');
    if (qp != std::string::npos) url = url.substr(0, qp);
    
    std::string rel = (!url.empty() && url[0] == '/') ? url.substr(1) : url;
    std::string content = read_file_content(static_path_ + "/" + rel);
    
    std::string response;
    if (content.empty()) {
        response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nNot found";
    } else {
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: " + get_mime(rel) + "\r\n";
        response += "Content-Length: " + std::to_string(content.size()) + "\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "Connection: close\r\n\r\n" + content;
    }
    
    send(client_fd, response.c_str(), response.size(), 0);
    close(client_fd);
}

void VizServer::run_sse_client(int client_fd) {
    // Send SSE headers
    std::string headers = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    send(client_fd, headers.c_str(), headers.size(), MSG_NOSIGNAL);
    
    std::cout << "[VizServer] SSE client connected, fd=" << client_fd << std::endl;
    
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        ws_clients_.insert(client_fd);
    }
    
    // Send initial snapshot
    send_snapshot_now(client_fd);
    
    // Keep alive loop
    char buf[256];
    while (running_.load()) {
        ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data from client (normal for SSE) — send pending snapshots
                handle_pending_snapshots();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            break; // Real error
        }
        if (n == 0) break;  // Connection closed
        handle_pending_snapshots();
    }
    
    std::lock_guard<std::mutex> lock(clients_mutex_);
    ws_clients_.erase(client_fd);
    close(client_fd);
}

void VizServer::run_server() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[VizServer] Failed to create socket" << std::endl;
        return;
    }
    
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(http_port_);
    
    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[VizServer] Failed to bind port " << http_port_ << std::endl;
        return;
    }
    
    if (listen(server_fd_, 128) < 0) {
        std::cerr << "[VizServer] Failed to listen" << std::endl;
        return;
    }
    
    int flags = fcntl(server_fd_, F_GETFL, 0);
    fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);
    
    std::cout << "[VizServer] HTTP server listening on port " << http_port_ << std::endl;
    
    while (running_.load()) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Accept-цикл не занимается отправкой — этим заняты SSE-потоки.
                // Просто ждём новых подключений.
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        int cflags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, cflags | O_NONBLOCK);
        
        char buf[8192];
        ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            close(client_fd);
            continue;
        }
        buf[n] = '\0';
        std::string request(buf, n);
        
        // Check for SSE stream endpoint
        if (request.find("/stream") != std::string::npos) {
            // Запускаем SSE в отдельном потоке, чтобы не блокировать accept()
            std::thread sse_thr(sse_thread_func, this, client_fd);
            {
                std::lock_guard<std::mutex> lk(sse_threads_mutex_);
                sse_threads_.push_back(std::move(sse_thr));
            }
        } else if (is_websocket_handshake(request)) {
            handle_websocket_upgrade(client_fd, request);
        } else {
            serve_http(client_fd, request);
        }
    }
    
    close(server_fd_);
    server_fd_ = -1;

    // Ждём завершения всех SSE-потоков
    {
        std::lock_guard<std::mutex> lk(sse_threads_mutex_);
        for (auto& t : sse_threads_) {
            if (t.joinable()) t.join();
        }
        sse_threads_.clear();
    }
}

} // namespace s2
