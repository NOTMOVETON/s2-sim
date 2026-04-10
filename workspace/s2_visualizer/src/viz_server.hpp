#pragma once

#include <s2/world_snapshot.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <set>
#include <optional>
#include <cstdint>
#include <functional>

namespace s2 {

/**
 * @brief Обработчик команд от визуализатора.
 */
struct VizCommandHandler {
    virtual ~VizCommandHandler() = default;

    virtual void on_pause() = 0;
    virtual void on_resume() = 0;
    virtual void on_reset() = 0;
    virtual void on_move_agent(AgentId id, double x, double y, double yaw) = 0;
    virtual void on_plugin_input(AgentId agent_id, const std::string& plugin_type, const std::string& json_input) = 0;
};

/**
 * @brief WebSocket + HTTP сервер для визуализатора.
 *
 * Один порт (http_port_): HTTP — статика, SSE — подключение браузера.
 * При publish() — отправляет JSON всем подключённым клиентам.
 * При первом подключении клиента — отправляет снапшот с геометрией.
 *
 * Поток-безопасен: publish() копирует снапшот через мьютекс.
 */
class VizServer {
public:
    /**
     * @param ws_port       Игнорируется (объединено с http_port)
     * @param http_port     Порт сервера (HTTP + SSE)
     * @param static_path   Путь к директории со статическими файлами (web/)
     */
    VizServer(int ws_port, int http_port, const std::string& static_path);
    ~VizServer();

    VizServer(const VizServer&) = delete;
    VizServer& operator=(const VizServer&) = delete;

    /** Запустить сервер в отдельном потоке */
    void start();

    /** Остановить сервер, подождать завершения потока */
    void stop();

    /** Опубликовать снапшот всем подключённым клиентам */
    void publish(const WorldSnapshot& snapshot);

    /** Мгновенная отправка снапшота всем SSE-клиентам (без буферизации) */
    void force_broadcast(const WorldSnapshot& snapshot);

    /** Отправить последний снапшот всем SSE-клиентам */
    void force_broadcast_latest();

    /** Установить обработчик команд */
    void set_command_handler(VizCommandHandler* handler) { command_handler_ = handler; }

    /** Получить команды от визуализатора (заглушка на будущее) */
    void poll_commands();

    /** Количество подключённых клиентов */
    int client_count() const;

    /** SSE поток — публичная для запуска из std::thread */
    void run_sse_client(int client_fd);

private:
    /** Точка входа серверного потока */
    void run_server();

    /** Проверить, является ли запрос WebSocket handshake */
    bool is_websocket_handshake(const std::string& request);

    /** Обработать WebSocket handshake и запустить WS цикл */
    void handle_websocket_upgrade(int client_fd, const std::string& request);

    /** Отдать статический файл по HTTP */
    void serve_http(int client_fd, const std::string& request);

    /** Послать текущий снапшот конкретному клиенту (при подключении) */
    void send_snapshot_now(int client_fd);

    /** Отправить все pending снапшоты всем клиентам (вызывается в главном цикле) */
    void handle_pending_snapshots();

    /** Цикл чтения WebSocket фреймов от клиента */
    void handle_websocket_loop(int client_fd);

    int ws_port_;
    int http_port_;
    std::string static_path_;

    std::thread thread_;
    std::atomic<bool> running_{false};

    mutable std::mutex snapshot_mutex_;
    WorldSnapshot pending_snapshot_;
    std::optional<WorldSnapshot> current_snapshot_;
    std::atomic<bool> has_pending_{false};

    mutable std::mutex clients_mutex_;
    std::set<int> ws_clients_;

    mutable std::mutex sse_threads_mutex_;
    std::vector<std::thread> sse_threads_;

    int server_fd_;

    VizCommandHandler* command_handler_ = nullptr;

    // Throttle plugins_data: включать только раз в PLUGIN_DATA_INTERVAL кадров (~3Hz при 30fps)
    std::atomic<int> snap_counter_{0};
    static constexpr int PLUGIN_DATA_INTERVAL = 10;
};

} // namespace s2