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

    T pop() {
        std::lock_guard queueLock{mutex_};
        T res = queue_.front();
        queue_.pop();
        return res;
    }

    uint32_t size() const {
        return queue_.size();
    }

  private:
    std::mutex mutex_;
    std::queue<T> queue_;
};