#pragma once
#include <s2/world_snapshot.hpp>
namespace s2 {
struct IVizAdapter {
    virtual ~IVizAdapter() = default;
    virtual void publish(const WorldSnapshot& snapshot) = 0;
    virtual void start() {}
    virtual void stop() {}
};
} // namespace s2
