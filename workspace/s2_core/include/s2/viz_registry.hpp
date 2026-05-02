#pragma once
#include <s2/viz_adapter.hpp>
#include <vector>
namespace s2 {
class VizRegistry final : public IVizAdapter {
public:
    void register_adapter(IVizAdapter* adapter) { adapters_.push_back(adapter); }
    void publish(const WorldSnapshot& snap) override {
        for (auto* a : adapters_) a->publish(snap);
    }
    void start() override { for (auto* a : adapters_) a->start(); }
    void stop()  override { for (auto* a : adapters_) a->stop(); }
private:
    std::vector<IVizAdapter*> adapters_;
};
} // namespace s2
