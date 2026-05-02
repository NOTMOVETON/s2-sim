#pragma once

#include <s2/kernel_command.hpp>
#include <mutex>
#include <queue>
#include <vector>

namespace s2 {

class CommandQueue
{
public:
    void enqueue(KernelCommand cmd)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(cmd));
    }

    std::vector<KernelCommand> drain()
    {
        std::queue<KernelCommand> local;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::swap(local, queue_);
        }
        std::vector<KernelCommand> result;
        result.reserve(local.size());
        while (!local.empty()) {
            result.push_back(std::move(local.front()));
            local.pop();
        }
        return result;
    }

private:
    std::queue<KernelCommand> queue_;
    std::mutex mutex_;
};

} // namespace s2
