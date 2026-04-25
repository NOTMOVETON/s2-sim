#pragma once
#include <s2/types.hpp>

namespace s2 {

/// Данные о runtime-точке назначения для телепорта.
/// Хранится в ZoneSystem и обновляется через SetZoneTeleportTargetCommand.
struct TeleportTargetData {
    Vec3 destination{Vec3::Zero()};
    double destination_yaw{0.0};
    bool has_destination{false};
};

} // namespace s2
