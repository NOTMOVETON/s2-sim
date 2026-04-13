#pragma once

/**
 * @file topic_display.hpp
 * Плагин отображения данных с произвольного ROS2-топика типа std_msgs/String.
 *
 * Подписывается на топик, который публикует строку в формате JSON, и передаёт
 * последнее полученное значение во фронтенд через to_json() → plugins_data → SSE.
 *
 * Несколько экземпляров с разными топиками подключаются независимо:
 * каждый указывает свой topic: в конфиге.
 *
 * Конфиг YAML:
 *   - type: "topic_display"
 *     topic: "/robot_state"    # ROS2-топик std_msgs/String
 */

#include <s2/plugins/plugin_base.hpp>
#include <nlohmann/json.hpp>
#include <mutex>
#include <string>

namespace s2
{
namespace plugins
{

class TopicDisplayPlugin : public IAgentPlugin
{
public:
    std::string type() const override { return "topic_display"; }

    void from_config(const YAML::Node& node) override
    {
        if (node["topic"])
            topic_ = node["topic"].as<std::string>();
    }

    void update(double /*dt*/, Agent& /*agent*/) override {}

    // ─── Подписка ──────────────────────────────────────────────────────────

    std::vector<std::string> subscribe_topics() const override
    {
        return {topic_};
    }

    std::string subscription_msg_type() const override
    {
        return "std_msgs/String";
    }

    /**
     * @brief Обрабатывает входящее std_msgs/String в JSON.
     *
     * Ожидаемый формат (конвертируется в ros2_transport_adapter.cpp):
     * {"data": "<строка — JSON-контент сообщения>"}
     */
    void handle_subscription(const std::string& /*topic*/,
                              const std::string& msg_json) override
    {
        try
        {
            YAML::Node wrapper = YAML::Load(msg_json);
            std::lock_guard<std::mutex> lock(mutex_);
            if (wrapper["data"])
                raw_ = wrapper["data"].as<std::string>();
        }
        catch (const std::exception&) {}
    }

    // ─── Экспорт в UI ──────────────────────────────────────────────────────

    /**
     * @brief Возвращает JSON для отображения в аккордеоне UI.
     *
     * Если raw_ является валидным JSON — встраивает его как объект/массив.
     * Иначе — возвращает как строку (failsafe).
     *
     * Формат: {"topic": "/...", "data": <json или строка>}
     */
    std::string to_json() const override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        nlohmann::json result;
        result["topic"] = topic_;
        if (!raw_.empty())
        {
            auto parsed = nlohmann::json::parse(raw_, nullptr, /*allow_exceptions=*/false);
            result["data"] = parsed.is_discarded() ? nlohmann::json(raw_) : parsed;
        }
        else
        {
            result["data"] = nullptr;
        }
        return result.dump();
    }

private:
    std::string topic_{"/topic"};

    mutable std::mutex mutex_;
    std::string raw_;
};

} // namespace plugins
} // namespace s2
