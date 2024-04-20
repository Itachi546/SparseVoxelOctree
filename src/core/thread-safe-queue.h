#pragma once

#include <queue>
#include <mutex>

template <typename T>
class ThreadSafeQueue {
  public:
    void push(T t) {
        std::lock_guard queueLock{mutex_};
        queue_.push(t);
    }

    bool empty() {
        std::lock_guard queueLock{mutex_};
        return queue_.size() == 0;
    }

    bool try_pop(T *res) {
        std::lock_guard queueLock{mutex_};
        if (queue_.size() > 0) {
            *res = queue_.front();
            queue_.pop();
            return true;
        }
        return false;
    }

    T pop() {
        std::lock_guard queueLock{mutex_};
        T res = queue_.front();
        queue_.pop();
        return res;
    }

    uint32_t size() {
        std::lock_guard queueLock{mutex_};
        return static_cast<uint32_t>(queue_.size());
    }

  private:
    std::mutex mutex_;
    std::queue<T> queue_;
};