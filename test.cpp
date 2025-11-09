#include "thread_pool.h"
#include <iostream>
#include <cassert>
#include <vector>

// Simple tests
void test_basic_submission() {
    std::cout << "test 1: basic";
    ThreadPool pool(4);
    
    auto future = pool.submit([]() { return 42; });
    assert(future.get() == 42);
    
    std::cout << " PASSED\n";
}

void test_multiple_tasks() {
    std::cout << "test 2: multiple tasks ";
    ThreadPool pool(4);
    
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.submit([i]() { return i * i; }));
    }
    
    for (int i = 0; i < 100; ++i) {
        assert(futures[i].get() == i * i);
    }
    
    std::cout << "PASSED\n";
}

void test_priorities() {
    std::cout << "test 3: prio tasks ";
    ThreadPool pool(2);
    
    auto low = pool.submit(Priority::LOW, []() { return 1; });
    auto med = pool.submit(Priority::MEDIUM, []() { return 2; });
    auto high = pool.submit(Priority::HIGH, []() { return 3; });
    
    assert(low.get() == 1);
    assert(med.get() == 2);
    assert(high.get() == 3);
    
    std::cout << "PASSED\n";
}

void test_exception_handling() {
    std::cout << "test 4: exceptions ";
    ThreadPool pool(4);
    
    auto future = pool.submit([]() -> int {
        throw std::runtime_error("test exception");
        return 42;
    });
    
    bool caught = false;
    try {
        future.get();
    } catch (const std::runtime_error&) {
        caught = true;
    }
    
    assert(caught);
    
    auto future2 = pool.submit([]() { return 100; });
    assert(future2.get() == 100);
    
    std::cout << "PASSED\n";
}

void test_wait_all() {
    std::cout << "test 5: wait_all";
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    
    for (int i = 0; i < 50; ++i) {
        pool.submit([&counter]() {
            counter++;
        });
    }
    
    pool.wait_all();
    assert(counter == 50);
    
    std::cout << "PASSED\n";
}

void test_statistics() {
    std::cout << "test 6: statistics ";
    ThreadPool pool(4);
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.submit([]() {
        }));
    }
    
    for (auto& f : futures) {
        f.get();
    }
    
    auto stats = pool.get_stats();
    assert(stats.tasks_completed == 100);
    assert(stats.total_tasks_submitted == 100);
    
    std::cout << "PASSED\n";
}

void test_shutdown_graceful() {
    std::cout << "test 7: shutdown ";
    
    ThreadPool pool(2);
    std::atomic<int> completed{0};
    
    for (int i = 0; i < 10; ++i) {
        pool.submit([&completed]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            completed++;
        });
    }
    
    pool.shutdown_graceful();
    assert(completed == 10);
    
    std::cout << "PASSED\n";
}

void test_shutdown_immediate() {
    std::cout << "test 8: immediate shutdown ";
    
    ThreadPool pool(2);
    std::atomic<int> completed{0};
    
    for (int i = 0; i < 100; ++i) {
        pool.submit([&completed]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            completed++;
        });
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pool.shutdown_immediate();
    
    std::cout << "PASSED (completed " << completed << " tasks before shutdown)\n";
}

int main() {
    std::cout << "Test suite\n";
    
    try {
        test_basic_submission();
        test_multiple_tasks();
        test_priorities();
        test_exception_handling();
        test_wait_all();
        test_statistics();
        test_shutdown_graceful();
        test_shutdown_immediate();
        
        std::cout << "ALL TESTS PASSED\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nFAILED: " << e.what() << "\n";
        return 1;
    }
}
