#pragma once
#include <s2/types.hpp>

namespace s2 {

/// Отложенный телепорт агента — обрабатывается SimEngine в конце тика.
/// Устанавливается TeleportEffect, применяется в фазе 3m SimEngine.
struct PendingTeleport {
    Vec3 destination;
    double yaw{0.0};
    bool pending{false};
};

} // namespace s2
