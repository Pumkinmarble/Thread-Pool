#include "thread_pool.h"
#include <iostream>
#include <algorithm>

ThreadPool::ThreadPool(size_t num_threads)
    : stop_(false)
    , immediate_stop_(false)
    , active_tasks_(0)
    , pending_tasks_(0)
    , tasks_completed_(0)
    , tasks_stolen_(0)
    , total_tasks_submitted_(0)
    , gen_(rd_()) {
    
    if (num_threads == 0) {
        throw std::invalid_argument("Thread pool must have at least one thread");
    }
    
    for (size_t i = 0; i < num_threads; ++i) {
        local_queues_.emplace_back(std::make_unique<WorkStealingQueue>());
    }
    
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this, i] { worker_thread(i); });
    }
}

ThreadPool::~ThreadPool() {
    shutdown_graceful();
}

void ThreadPool::worker_thread(size_t thread_id) {
    while (true) {
        std::function<void()> task;
        
        if (get_task(thread_id, task)) {
            task();
        } else {
            if (immediate_stop_) {
                break;
            }
            
            if (stop_ && pending_tasks_ == 0) {
                break;
            }
            
            std::unique_lock<std::mutex> lock(global_mutex_);
            global_cv_.wait_for(lock, std::chrono::milliseconds(10), [this] {
                return stop_ || immediate_stop_ || !global_queue_.empty() || pending_tasks_ > 0;
            });
            
            if (immediate_stop_) {
                break;
            }
            
            if (stop_ && pending_tasks_ == 0) {
                break;
            }
        }
    }
}

bool ThreadPool::get_task(size_t thread_id, std::function<void()>& task) {
    {
        std::lock_guard<std::mutex> lock(global_mutex_);
        if (!global_queue_.empty()) {
            task = std::move(global_queue_.top().task);
            global_queue_.pop();
            return true;
        }
    }
    
    if (local_queues_[thread_id]->pop(task)) {
        return true;
    }
    
    if (try_steal(thread_id, task)) {
        tasks_stolen_++;
        return true;
    }
    
    return false;
}

bool ThreadPool::try_steal(size_t thread_id, std::function<void()>& task) {
    size_t num_threads = workers_.size();
    
    if (num_threads == 1) {
        return false;
    }
    
    std::uniform_int_distribution<size_t> dist(0, num_threads - 1);
    size_t start = dist(gen_);
    
    for (size_t i = 0; i < num_threads; ++i) {
        size_t target = (start + i) % num_threads;
        
        if (target == thread_id) {
            continue;
        }
        
        if (local_queues_[target]->steal(task)) {
            return true;
        }
    }
    
    return false;
}

void ThreadPool::wait_all() {
    std::unique_lock<std::mutex> lock(wait_mutex_);
    wait_cv_.wait(lock, [this] {
        return pending_tasks_ == 0;
    });
}

size_t ThreadPool::active_tasks() const {
    return active_tasks_.load();
}

size_t ThreadPool::pending_tasks() const {
    return pending_tasks_.load();
}

void ThreadPool::shutdown_graceful() {
    if (stop_ || immediate_stop_) {
        return;
    }
    
    stop_ = true;
    global_cv_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::shutdown_immediate() {
    if (immediate_stop_) {
        return;
    }
    
    immediate_stop_ = true;
    stop_ = true;
    global_cv_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(global_mutex_);
        while (!global_queue_.empty()) {
            global_queue_.pop();
        }
    }
    
    for (auto& queue : local_queues_) {
        std::function<void()> dummy;
        while (queue->pop(dummy)) {
        }
    }
    
    pending_tasks_ = 0;
}

size_t ThreadPool::num_threads() const {
    return workers_.size();
}

ThreadPool::Stats ThreadPool::get_stats() const {
    return Stats{
        tasks_completed_.load(),
        tasks_stolen_.load(),
        total_tasks_submitted_.load()
    };
}
