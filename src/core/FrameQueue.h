#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>

namespace ors {

// Bounded queue. Video uses push() (drop-oldest when full). Audio uses
// pushWait() so PCM is never dropped; wake() unblocks waiters on stop.
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
        cv_.notify_one();
    }

    // Blocks while full instead of dropping. Returns false if wake() was called.
    bool pushWait(T frame)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return queue_.size() < capacity_ || stopping_; });
        if (stopping_) {
            return false;
        }
        queue_.push_back(std::move(frame));
        cv_.notify_one();
        return true;
    }

    bool pop(T& out)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        out = std::move(queue_.front());
        queue_.pop_front();
        cv_.notify_one();
        return true;
    }

    bool waitPop(T& out, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this] { return !queue_.empty() || stopping_; })) {
            return false;
        }
        if (queue_.empty()) {
            return false;
        }
        out = std::move(queue_.front());
        queue_.pop_front();
        cv_.notify_one();
        return true;
    }

    void wake()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        cv_.notify_all();
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
    bool stopping_{false};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace ors
