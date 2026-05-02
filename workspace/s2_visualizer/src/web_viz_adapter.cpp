#include "web_viz_adapter.hpp"
namespace s2 {
WebVizAdapter::WebVizAdapter(int http_port, const std::string& static_path)
    : server_(0, http_port, static_path) {}
void WebVizAdapter::publish(const WorldSnapshot& snapshot) {
    server_.publish(snapshot);
}
void WebVizAdapter::start() { server_.start(); }
void WebVizAdapter::stop()  { server_.stop(); }
} // namespace s2
