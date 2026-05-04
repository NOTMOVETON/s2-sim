#pragma once

namespace s2 {

// Phase 9 сделает WorldContext полным (добавит WorldQuery*).
struct WorldContext {
    double sim_time{0.0};
    double dt{0.0};
};

} // namespace s2
