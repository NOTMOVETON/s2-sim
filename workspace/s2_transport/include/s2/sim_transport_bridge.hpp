#pragma once

/**
 * @file sim_transport_bridge.hpp
 * Мост между SimEngine и транспортным адаптером.
 *
 * SimTransportBridge:
 *  - При инициализации обходит агентов и регистрирует их в адаптере
 *    (топики команд, сервисы — из плагинов через command_topics()/service_names())
 *  - Устанавливает post-tick callback в SimEngine
 *  - В колбэке строит AgentSensorFrame из SharedState и вызывает publish_agent_frame()
 *  - Опрашивает plugin->poll_events() и вызывает emit_event()
 */

#include <s2/transport_adapter.hpp>
#include <s2/sim_engine.hpp>
#include <s2/geo_origin.hpp>

#include <map>
#include <memory>
#include <string>

namespace s2
{

/**
 * @brief Мост между SimEngine и ITransportAdapter.
 *
 * Создаётся в main() после загрузки сцены. Не владеет engine.
 */
class SimTransportBridge
{
public:
    /**
     * @param engine  Указатель на SimEngine (не владеет)
     * @param adapter Транспортный адаптер (shared ownership)
     */
    SimTransportBridge(SimEngine* engine,
                       std::shared_ptr<ITransportAdapter> adapter);

    /**
     * @brief Инициализировать мост.
     *
     * Регистрирует geo_origin, обходит агентов и плагины:
     *  - register_agent() для каждого агента
     *  - register_sensor() для каждого сенсорного плагина
     *  - register_input_topic() для каждого command_topic() плагина
     *  - register_service() для каждого service_name() плагина
     *
     * Устанавливает post_tick_callback в SimEngine.
     * Вызывать до start().
     */
    void init(const GeoOrigin& geo_origin);

    /**
     * @brief Запустить транспортный адаптер.
     */
    void start();

    /**
     * @brief Остановить транспортный адаптер.
     */
    void stop();

    /**
     * @brief Post-tick callback для SimEngine.
     *
     * Строит AgentSensorFrame с sensors[], включая только плагины с изменившимся seq.
     * TF публикуется каждый кадр.
     * Вызывается из SimEngine с частотой transport_rate.
     */
    void on_post_tick(const SimWorld& world, double sim_time);

private:
    SimEngine*                         engine_;
    std::shared_ptr<ITransportAdapter> adapter_;

    // Типы плагинов, которые мы считаем сенсорными (для register_sensor)
    static bool is_sensor_plugin(const std::string& plugin_type);

    // Последние опубликованные seq per agent, per plugin (type:name)
    struct SensorKey { AgentId agent_id; std::string plugin_type; std::string plugin_name; };
    struct SensorKeyLess {
        bool operator()(const SensorKey& a, const SensorKey& b) const {
            if (a.agent_id != b.agent_id) return a.agent_id < b.agent_id;
            if (a.plugin_type != b.plugin_type) return a.plugin_type < b.plugin_type;
            return a.plugin_name < b.plugin_name;
        }
    };
    std::map<SensorKey, uint64_t, SensorKeyLess> last_published_seq_;
};

} // namespace s2
