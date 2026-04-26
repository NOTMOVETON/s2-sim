#include <s2/zone_spawn_system.hpp>
#include <s2/event_bus.hpp>

namespace s2 {

void ZoneSpawnSystem::init(SimBus& bus, KernelCommandQueue& out_queue, double /*current_sim_time*/)
{
    out_queue_ = &out_queue;

    // Подписка на EventBus события для event-триггеров.
    // Используем типизированную подписку на каждый известный тип.
    // При совпадении event_type в шаблоне — push SpawnZone в очередь.

    bus.subscribe<event::ZoneEntered>([this](const event::ZoneEntered& e) {
        check_event_triggers("ZoneEntered", std::to_string(e.entity_id));
    });

    bus.subscribe<event::ZoneExited>([this](const event::ZoneExited& e) {
        check_event_triggers("ZoneExited", std::to_string(e.entity_id));
    });

    bus.subscribe<event::SignalActivated>([this](const event::SignalActivated& e) {
        check_event_triggers("SignalActivated", std::to_string(e.source_entity));
    });

    bus.subscribe<event::GrabSucceeded>([this](const event::GrabSucceeded& e) {
        check_event_triggers("GrabSucceeded", std::to_string(e.agent));
    });

    bus.subscribe<event::GrabFailed>([this](const event::GrabFailed& e) {
        check_event_triggers("GrabFailed", std::to_string(e.agent));
    });

    // Подписка на ActorStateChanged для state_change триггеров
    bus.subscribe<event::ActorStateChanged>([this](const event::ActorStateChanged& e) {
        check_state_change_triggers(e.actor, e.new_state);
    });
}

void ZoneSpawnSystem::add_template(ZoneTemplate tmpl, double current_sim_time)
{
    if (tmpl.trigger_type == ZoneTemplate::TriggerType::TIMER) {
        tmpl.timer.trigger_time = current_sim_time + tmpl.timer.delay_seconds;
    }
    templates_.push_back(std::move(tmpl));
}

void ZoneSpawnSystem::tick(double sim_time)
{
    if (!out_queue_) return;
    for (auto& tmpl : templates_) {
        if (tmpl.trigger_type != ZoneTemplate::TriggerType::TIMER) continue;
        if (tmpl.timer.fired) continue;
        if (sim_time >= tmpl.timer.trigger_time) {
            out_queue_->push_back(tmpl.spawn_cmd);
            tmpl.timer.fired = true;
        }
    }
}

void ZoneSpawnSystem::clear()
{
    templates_.clear();
}

void ZoneSpawnSystem::check_event_triggers(const std::string& event_type,
                                            const std::string& source_entity)
{
    if (!out_queue_) return;
    for (auto& tmpl : templates_) {
        if (tmpl.trigger_type != ZoneTemplate::TriggerType::EVENT) continue;
        if (tmpl.event.event_type != event_type) continue;
        if (!tmpl.event.source_entity.empty() &&
            tmpl.event.source_entity != source_entity) continue;
        out_queue_->push_back(tmpl.spawn_cmd);
    }
}

void ZoneSpawnSystem::check_state_change_triggers(ActorId actor_id,
                                                   const std::string& new_state)
{
    if (!out_queue_) return;
    for (auto& tmpl : templates_) {
        if (tmpl.trigger_type != ZoneTemplate::TriggerType::STATE_CHANGE) continue;
        if (tmpl.state_change.actor_id != actor_id) continue;
        if (tmpl.state_change.target_state != new_state) continue;
        out_queue_->push_back(tmpl.spawn_cmd);
    }
}

} // namespace s2
