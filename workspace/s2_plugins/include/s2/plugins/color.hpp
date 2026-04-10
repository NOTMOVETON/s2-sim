#pragma once

/**
 * @file color.hpp
 * Плагин смены цвета агента по ROS2-сервису (триггер).
 *
 * Сервис — простой триггер без параметров. Цвет и длительность задаются
 * в конфиге плагина. При вызове сервиса применяется configured_color_
 * на duration_ секунд, затем цвет восстанавливается.
 *
 * Для URDF-роботов (agent.kinematic_tree != nullptr) меняет цвет всех звеньев
 * с визуальной геометрией. Для простых роботов — agent.visual.color.
 */

#include <s2/plugins/plugin_base.hpp>
#include <s2/agent.hpp>
#include <s2/kinematic_tree.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace s2
{
namespace plugins
{

class ColorPlugin : public IAgentPlugin
{
public:
    std::string type() const override { return "color"; }

    std::vector<std::string> service_names() const override
    {
        return {service_name_};
    }

    /**
     * Запоминает исходные цвета агента до первого update().
     *  - URDF-робот: цвета всех звеньев с визуальной геометрией
     *  - простой робот: agent.visual.color
     */
    void initialize(Agent& agent) override;

    /**
     * Поля YAML:
     *   service:  /set_color    (по умолчанию)
     *   color:    "#FF0000"     (обязательно — цвет при триггере)
     *   duration: 5.0           (обязательно — длительность в секундах)
     */
    void from_config(const YAML::Node& node) override;

    /**
     * Триггер: запускает смену цвета.
     * Тело запроса игнорируется — параметры берутся из конфига.
     * Ответ: {"success": true}
     */
    std::string handle_service(const std::string& service_name,
                               const std::string& request_json) override;

    /**
     * timer_ > 0: применяет configured_color_, уменьшает таймер.
     * timer_ <= 0: восстанавливает исходный цвет.
     */
    void update(double dt, Agent& agent) override;

    /**
     * {"plugin":"color","active_color":"#FF0000","remaining":2.7}
     */
    std::string to_json() const override;

private:
    std::string service_name_{"/set_color"};
    std::string configured_color_{"#FF0000"};  ///< Цвет из конфига
    double      duration_{3.0};                ///< Длительность из конфига (секунды)

    // Исходный цвет для простых роботов (сохраняется в initialize)
    std::string default_color_;

    // Для URDF-роботов: имя звена → исходный цвет
    std::unordered_map<std::string, std::string> default_link_colors_;

    double timer_{0.0};      ///< Оставшееся время смены цвета
    bool is_urdf_{false};    ///< true = робот с kinematic_tree
};

} // namespace plugins
} // namespace s2
