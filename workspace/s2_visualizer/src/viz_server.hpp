#pragma once

#include <s2/world_snapshot.hpp>
#include <s2/world.hpp>
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

    /** Обновить статическую геометрию мира (заменяет текущий список примитивов). */
    virtual void on_update_geometry(const std::vector<WorldPrimitive>& prims) = 0;

    /** Переместить центр зоны по id (x, y, z — sim-координаты). */
    virtual bool on_move_zone(const std::string& zone_id, double x, double y, double z) { (void)zone_id; (void)x; (void)y; (void)z; return false; }

    /** Включить/выключить зону. */
    virtual bool on_toggle_zone(const std::string& zone_id, bool enabled) { (void)zone_id; (void)enabled; return false; }

    /** Обновить визуальные свойства зоны (цвет, прозрачность). */
    virtual bool on_update_zone_visual(const std::string& zone_id, const std::string& color, double opacity) { (void)zone_id; (void)color; (void)opacity; return false; }

    /** Создать новую зону (SpawnZone). */
    virtual bool on_spawn_zone(const std::string& shape_type, double cx, double cy,
                               double radius, double hx, double hy, double hz,
                               double cyl_r, double cyl_h,
                               const std::vector<std::string>& effects,
                               const std::string& color, double opacity,
                               const std::string& id_hint) {
        (void)shape_type; (void)cx; (void)cy; (void)radius;
        (void)hx; (void)hy; (void)hz; (void)cyl_r; (void)cyl_h;
        (void)effects; (void)color; (void)opacity; (void)id_hint;
        return false;
    }

    /** Изменить форму существующей зоны (resize_zone). */
    virtual bool on_resize_zone(const std::string& zone_id,
                                const std::string& shape_type,
                                double radius, double hx, double hy, double hz,
                                double cyl_r, double cyl_h) {
        (void)zone_id; (void)shape_type; (void)radius;
        (void)hx; (void)hy; (void)hz; (void)cyl_r; (void)cyl_h;
        return false;
    }

    /** Удалить зону (DespawnZone). */
    virtual bool on_despawn_zone(const std::string& zone_id) {
        (void)zone_id; return false;
    }

    /** Результат сохранения сцены в YAML. */
    struct SaveSceneResult {
        bool ok = false;
        std::string path_or_error; ///< Путь к файлу при успехе, сообщение об ошибке при неудаче
    };

    /** Сохранить текущую сцену (геометрию) в YAML-файл на диске. */
    virtual SaveSceneResult on_save_scene() = 0;

    /**
     * @brief Получить текущее состояние сцены (агенты + геометрия) как JSON-строку.
     *
     * Читает из YAML-файла сцены. Используется редактором для загрузки текущих агентов.
     * Формат ответа: {"yaml_path":"...","agents":[...],"geometry":[...]}
     */
    virtual std::string on_get_scene_state() { return "{\"agents\":[],\"geometry\":[]}"; }

    /**
     * @brief Получить список URDF-файлов как JSON-строку.
     *
     * Сканирует директорию robots/ рядом со сценой.
     * Формат ответа: {"files":["dozer.urdf",...]}
     */
    virtual std::string on_get_urdf_list() { return "{\"files\":[]}"; }

    /**
     * @brief Сохранить обновлённый список агентов в YAML-файл сцены.
     *
     * @param agents_json  JSON-строка: массив агентов [{"name":"robot_0",...},...]
     */
    virtual SaveSceneResult on_update_agents(const std::string& agents_json)
    {
        (void)agents_json;
        return {false, "not implemented"};
    }

    /**
     * @brief Получить список .yaml файлов из директории сцен.
     *
     * Формат ответа: {"scenes":["test_basic.yaml","test_collision.yaml",...]}
     */
    virtual std::string on_get_scene_list() { return "{\"scenes\":[]}"; }

    /**
     * @brief Перезагрузить симуляцию с новой сценой.
     *
     * Паузит движок, загружает новую сцену из файла, перезапускает.
     * @param filename Имя файла (только basename, без пути), например "test_basic.yaml"
     */
    virtual SaveSceneResult on_load_scene(const std::string& filename)
    {
        (void)filename;
        return {false, "not implemented"};
    }

    /**
     * @brief Сохранить текущую сцену под новым именем.
     *
     * @param new_name Новое имя (без пути, с .yaml или без)
     */
    virtual SaveSceneResult on_save_scene_as(const std::string& new_name)
    {
        (void)new_name;
        return {false, "not implemented"};
    }

    /**
     * @brief Создать новую пустую сцену и загрузить её.
     *
     * @param new_name Имя новой сцены
     */
    virtual SaveSceneResult on_new_scene(const std::string& new_name)
    {
        (void)new_name;
        return {false, "not implemented"};
    }
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

    /** Отправить последний снапшот с геометрией всем SSE-клиентам (после обновления геометрии) */
    void force_broadcast_with_geometry();

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