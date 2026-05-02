#pragma once
#include <s2/viz_adapter.hpp>
namespace s2 {
struct NullVizAdapter final : IVizAdapter {
    void publish(const WorldSnapshot&) override {}
};
} // namespace s2
