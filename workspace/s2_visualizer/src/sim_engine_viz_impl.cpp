#include <s2/sim_engine.hpp>
#include "viz_server.hpp"

namespace s2 {

void SimEngine::publish_viz() {
    if (!viz_server_) return;
    viz_server_->publish(build_snapshot());
}

} // namespace s2