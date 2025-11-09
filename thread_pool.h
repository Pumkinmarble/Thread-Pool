#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <deque>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <atomic>
#include <random>

// task prio levels
enum class Priority {
    HIGH = 0,
    MEDIUM = 1,
    LOW = 2
};

class ThreadPool;

class WorkStealingQueue {
private:
    std::deque<std::function<void()>> queue_;
    mutable std::mutex mutex_;

public:
    WorkStealingQueue() = default;
    
    void push(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_front(std::move(task));
    }
    
    bool pop(std::function<void()>& task) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        task = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }
    
    bool steal(std::function<void()>& task) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        task = std::move(queue_.back());
        queue_.pop_back();
        return true;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
};

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads);
    
    ~ThreadPool();
    
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>;
    
    template<class F, class... Args>
    auto submit(Priority priority, F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>;
    
    void wait_all();
    
    size_t active_tasks() const;
    
    size_t pending_tasks() const;
    
    void shutdown_graceful();
    
    void shutdown_immediate();
    
    size_t num_threads() const;
    
    struct Stats {
        size_t tasks_completed;
        size_t tasks_stolen;
        size_t total_tasks_submitted;
    };
    
    Stats get_stats() const;

private:
    void worker_thread(size_t thread_id);
    
    bool get_task(size_t thread_id, std::function<void()>& task);
    
    bool try_steal(size_t thread_id, std::function<void()>& task);
    
    struct PriorityTask {
        Priority priority;
        std::function<void()> task;
        
        bool operator<(const PriorityTask& other) const {
            return priority > other.priority;
        }
    };
    
    std::vector<std::thread> workers_;
    std::vector<std::unique_ptr<WorkStealingQueue>> local_queues_;
    
    std::priority_queue<PriorityTask> global_queue_;
    std::mutex global_mutex_;
    std::condition_variable global_cv_;
    
    std::atomic<bool> stop_;
    std::atomic<bool> immediate_stop_;
    std::atomic<size_t> active_tasks_;
    std::atomic<size_t> pending_tasks_;
    
    std::atomic<size_t> tasks_completed_;
    std::atomic<size_t> tasks_stolen_;
    std::atomic<size_t> total_tasks_submitted_;
    
    std::random_device rd_;
    std::mt19937 gen_;
    
    std::condition_variable wait_cv_;
    std::mutex wait_mutex_;
};

template<class F, class... Args>
auto ThreadPool::submit(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    return submit(Priority::MEDIUM, std::forward<F>(f), std::forward<Args>(args)...);
}

template<class F, class... Args>
auto ThreadPool::submit(Priority priority, F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    
    using return_type = typename std::invoke_result<F, Args...>::type;
    
    if (stop_ || immediate_stop_) {
        throw std::runtime_error("Cannot submit task to stopped thread pools");
    }
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    auto wrapped_task = [this, task]() {
        try {
            (*task)();
        } catch (...) {
        }
        active_tasks_--;
        pending_tasks_--;
        tasks_completed_++;
        wait_cv_.notify_all();
    };
    
    active_tasks_++;
    pending_tasks_++;
    total_tasks_submitted_++;
    
    if (priority == Priority::HIGH) {
        std::lock_guard<std::mutex> lock(global_mutex_);
        global_queue_.push({priority, std::move(wrapped_task)});
        global_cv_.notify_one();
    } else {
        static std::atomic<size_t> counter{0};
        size_t thread_id = counter++ % workers_.size();
        local_queues_[thread_id]->push(std::move(wrapped_task));
        global_cv_.notify_one();
    }
    
    return result;
}

#endif
