#pragma once

#include <vector>
#include <mutex>

template <typename T>
class ThreadSafeVector {
  public:
    void push(T t) {
        std::lock_guard queueLock{mutex_};
        data_.push_back(t);
    }

    T &get(uint32_t index) {
        return data_[index];
    }

    void reset() {
        data_.clear();
    }

    uint32_t size() {
        return static_cast<uint32_t>(data_.size());
    }

  private:
    std::mutex mutex_;
    std::vector<T> data_;
};