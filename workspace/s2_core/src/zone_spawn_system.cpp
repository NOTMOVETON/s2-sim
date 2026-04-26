#include <s2/zone_spawn_system.hpp>
#include <s2/event_bus.hpp>

namespace s2 {

void ZoneSpawnSystem::init(SimBus& /*bus*/, KernelCommandQueue& out_queue, double /*current_sim_time*/)
{
    out_queue_ = &out_queue;
    // TODO: подписки на EventBus
}

void ZoneSpawnSystem::add_template(ZoneTemplate tmpl, double /*current_sim_time*/)
{
    templates_.push_back(std::move(tmpl));
}

void ZoneSpawnSystem::tick(double /*sim_time*/)
{
    // TODO: проверить timer-триггеры
}

void ZoneSpawnSystem::clear()
{
    templates_.clear();
}

void ZoneSpawnSystem::check_event_triggers(const std::string& /*event_type*/,
                                            const std::string& /*source_entity*/)
{
    // TODO: проверить event-триггеры
}

void ZoneSpawnSystem::check_state_change_triggers(ActorId /*actor_id*/,
                                                   const std::string& /*new_state*/)
{
    // TODO: проверить state_change триггеры
}

} // namespace s2
