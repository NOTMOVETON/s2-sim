#pragma once
#include <s2/viz_adapter.hpp>
#include "viz_server.hpp"
#include <string>
namespace s2 {
class WebVizAdapter final : public IVizAdapter {
public:
    WebVizAdapter(int http_port, const std::string& static_path);
    void publish(const WorldSnapshot& snapshot) override;
    void start() override;
    void stop() override;
    VizServer& server() { return server_; }
private:
    VizServer server_;
};
} // namespace s2
