#pragma once

#include <s2/types.hpp>
#include <string>

namespace s2 {

/**
 * @brief Контекст, передаваемый плагину эффекта при каждом вызове.
 *
 * Содержит всё необходимое для time-based, positional и рандомизированных эффектов.
 * Не хранит указатели на агента или зону — только значения, чтобы избежать
 * проблем с висячими ссылками при асинхронных вызовах.
 */
struct EffectContext
{
    double sim_time{0.0};              ///< Время с начала симуляции (сек)
    double dt{0.0};                    ///< Шаг тика (сек)

    ZoneId zone_id;                    ///< ID зоны
    Vec3 zone_center{Vec3::Zero()};    ///< Текущий центр зоны
    Vec3 zone_half_size{Vec3::Zero()}; ///< Текущие полуразмеры зоны (для AABB)

    EntityId entity_id{0};             ///< Универсальный ID сущности (Phase 2+)
    AgentId  agent_id{0};              ///< ID агента (backward-compat alias; Phase 6 уберёт)
    Vec3 agent_position{Vec3::Zero()}; ///< Текущая позиция агента
};

} // namespace s2
