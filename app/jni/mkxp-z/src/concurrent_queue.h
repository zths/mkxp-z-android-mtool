#ifndef CONCURRENT_QUEUE_H
#define CONCURRENT_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class concurrent_queue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;

public:
    concurrent_queue() = default;
    concurrent_queue(const concurrent_queue&) = delete;
    concurrent_queue& operator=(const concurrent_queue&) = delete;

    void push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(std::move(item));
        lock.unlock();
        cond_.notify_one();
    }

    bool try_pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.empty()) {
            cond_.wait(lock);
        }
        item = std::move(queue_.front());
        queue_.pop();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
};

#endif // CONCURRENT_QUEUE_H
