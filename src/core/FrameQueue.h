#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>

namespace ors {

// Bounded queue: when full, the oldest item is dropped.
template <typename T>
class FrameQueue {
public:
    static constexpr std::size_t kDefaultCapacity = 4;

    explicit FrameQueue(std::size_t capacity = kDefaultCapacity)
        : capacity_(capacity == 0 ? kDefaultCapacity : capacity)
    {}

    void push(T frame)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= capacity_) {
            queue_.pop_front();
            ++dropped_;
        }
        queue_.push_back(std::move(frame));
    }

    bool pop(T& out)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        out = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    std::size_t capacity() const { return capacity_; }

    std::uint64_t dropped() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return dropped_;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
    }

private:
    std::size_t capacity_;
    std::deque<T> queue_;
    std::uint64_t dropped_{0};
    mutable std::mutex mutex_;
};

} // namespace ors
