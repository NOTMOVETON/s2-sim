#pragma once

#include <s2/agent.hpp>
#include <s2/actor.hpp>
#include <s2/sim_bus.hpp>
#include <s2/zone.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace s2 {

/// Результат проверки вхождения агента в зону.
struct InZoneResult {
    bool inside{false};
    std::string contact_link; ///< Имя первого линка внутри зоны (PER_LINK), пусто для CENTER/BOUNDING
};

/// Фабрика создаёт плагин эффекта по строковому типу и YAML-параметрам.
using EffectFactory = std::function<
    std::unique_ptr<EffectPlugin>(const std::string& type, const YAML::Node& params)>;

/**
 * @brief Система управления зонами и эффектами.
 *
 * Хранит все зоны сцены и каждый тик:
 *  1. Обновляет позиции зон, привязанных к актору/агенту.
 *  2. Проверяет входы/выходы агентов — публикует события на SimBus.
 *  3. Применяет MODIFIER и CONTINUOUS эффекты к агентам внутри зон.
 *
 * MUTATION-эффект применяется однократно при входе (on_agent_enter).
 * При выходе MUTATION не отменяется.
 */
class ZoneSystem
{
public:
    /// Установить фабрику эффектов.
    /// Если фабрика задана — on_init() вызывается для каждого плагина при add_zone().
    void set_effect_factory(EffectFactory factory);

    /// Добавить зону. Если фабрика задана — создаёт плагины и вызывает on_init().
    void add_zone(Zone zone);

    /**
     * @brief Основной тик системы зон.
     *
     * Порядок:
     *  1. Обновить позиции attached-зон.
     *  2. Для каждой зоны: проверить enter/exit для каждого агента.
     *  3. Применить активные эффекты (MODIFIER, CONTINUOUS) для агентов внутри.
     *
     * @param agents   Все агенты мира (изменяются через SharedState)
     * @param actors   Акторы для привязки зон
     * @param bus      Шина событий
     * @param sim_time Текущее симуляционное время
     * @param dt       Шаг тика
     */
    void tick(
        std::vector<Agent>& agents,
        const std::vector<Actor>& actors,
        SimBus& bus,
        double sim_time,
        double dt);

    // ── Kernel Commands ──────────────────────────────────────────────────────

    /// Изменить геометрию зоны. Возвращает false если зона не найдена.
    bool resize_zone(const ZoneId& id, const ZoneShape& new_shape);

    /// Переместить центр (только для не-attached зон). Возвращает false если зона не найдена.
    bool move_zone(const ZoneId& id, const Vec3& new_center);

    /// Привязать зону к актору. Возвращает false если зона не найдена.
    bool attach_zone_to_actor(const ZoneId& id, ActorId actor_id, const Vec3& offset);

    /// Включить/выключить зону. Возвращает false если зона не найдена.
    bool toggle_zone(const ZoneId& id, bool enabled);

    /// Включить/выключить зону с отправкой enter/exit событий.
    /// При enabled=false: для всех inside_agents отправляется ZoneExited.
    /// При enabled=true: для всех агентов внутри геометрии отправляется ZoneEntered.
    bool toggle_zone_with_events(const ZoneId& id,
                                 bool enabled,
                                 std::vector<Agent>& agents,
                                 SimBus& bus);

    /// Удалить зону, отправив ZoneExited всем агентам внутри.
    void remove_zone(const ZoneId& id,
                     std::vector<Agent>& agents,
                     SimBus& bus);

    /// Включить/выключить конкретный эффект в зоне. Возвращает false если не найдено.
    bool toggle_effect(const ZoneId& id, size_t effect_idx, bool enabled);

    /// Изменить визуальные свойства зоны (цвет, прозрачность). Возвращает false если не найдена.
    bool update_zone_visual(const ZoneId& id, const std::string& color, double opacity);

    // ── World Query API ──────────────────────────────────────────────────────

    /// Список ID зон, содержащих данную точку (только enabled-зоны).
    std::vector<ZoneId> zones_containing(const Vec3& point) const;

    /// Константный доступ ко всем зонам (для snapshot и тестов).
    const std::vector<Zone>& all_zones() const { return zones_; }

    /// Обновить позиции зон привязанных к entity.
    /// Вызывается в Phase 6 тика (attachments) из SimEngine::phase6_attachments().
    void update_owned_zones_positions(const std::vector<Agent>& agents);

private:
    /// Обновить lifecycle всех зон: рост и затухание strength.
    void update_lifecycle(double sim_time, double dt);

    /// Расширенная проверка: возвращает InZoneResult с contact_link для PER_LINK.
    static InZoneResult agent_in_zone_result(const Agent& agent, const Zone& zone);

    /// Проверить вхождение агента в зону с учётом detection_mode_enum.
    static bool agent_in_zone(const Agent& agent, const Zone& zone) {
        return agent_in_zone_result(agent, zone).inside;
    }

    /// Точка обнаружения для агента в зависимости от режима (legacy, string-based).
    /// "center" → agent.world_pose.position()
    /// "bounding" → fallback на center (реализация bounding overlap — задача позже)
    static Vec3 detection_point(const Agent& agent, const std::string& mode);

    /// Проверить capabilities-матч между агентом и списком требований.
    /// Возвращает true если required пуст ИЛИ все capabilities есть у агента.
    static bool capabilities_match(const Agent& agent,
                                   const std::vector<std::string>& required);

    /// Обработка входа агента в зону: публикация события + MUTATION.
    void on_agent_enter(Agent& agent, Zone& zone, SimBus& bus,
                        double sim_time, double dt);

    /// Обработка выхода агента из зоны: публикация события.
    void on_agent_exit(Agent& agent, Zone& zone, SimBus& bus);

    /// Применить MODIFIER и CONTINUOUS эффекты к агенту, находящемуся в зоне.
    void apply_active_effects(Agent& agent, Zone& zone,
                              double sim_time, double dt);

    std::vector<Zone> zones_;
    EffectFactory effect_factory_;
};

} // namespace s2
