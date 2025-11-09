#include "thread_pool.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <cmath>
#include <iomanip>

template<typename Func>
double measure_time(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

void example_basic() {
    std::cout << "\nexample 1: basic\n";
    
    ThreadPool pool(4);
    
    auto future1 = pool.submit([]() {
        std::cout << "task 1 executing " << std::this_thread::get_id() << "\n";
        return 42;
    });
    
    auto future2 = pool.submit([]() {
        std::cout << "task 2 executing " << std::this_thread::get_id() << "\n";
        return 100;
    });
    std::cout << "\n";
    std::cout << "result 1: " << future1.get() << "\n";
    std::cout << "result 2: " << future2.get() << "\n";
}

void example_priority() {
    std::cout << "\nexample 2: prio tasks\n";
    
    ThreadPool pool(2);
    
    std::cout << "\n";

    for (int i = 0; i < 5; ++i) {
        pool.submit(Priority::LOW, [i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::cout << "low priority task " << i << " completed\n";
        });
    }
    
    pool.submit(Priority::HIGH, []() {
        std::cout << "high prio task executed\n";
    });
    
    pool.wait_all();
}

void example_exceptions() {
    std::cout << "\nexample 3: exceptions\n";
    
    ThreadPool pool(4);
    
    auto future = pool.submit([]() -> int {
        throw std::runtime_error("task failed");
        return 42;
    });
    
    try {
        future.get();
    } catch (const std::exception& e) {
        std::cout << "caught exception: " << e.what() << "\n";
    }
    
    // NOTE: the pool should still work after exception
    auto future2 = pool.submit([]() {
        return 100;
    });
    
    std::cout << "pool still working, result: " << future2.get() << "\n";
}

void example_parallel_computation() {
    std::cout << "\nexample 4: parallel computation\n";
    
    const size_t N = 10000000;
    const size_t num_threads = 8;
    
    ThreadPool pool(num_threads);
    
    auto parallel_time = measure_time([&]() {
        std::vector<std::future<long long>> futures;
        size_t chunk_size = N / num_threads;
        
        for (size_t i = 0; i < num_threads; ++i) {
            size_t start = i * chunk_size;
            size_t end = (i == num_threads - 1) ? N : (i + 1) * chunk_size;
            
            futures.push_back(pool.submit([start, end]() {
                long long sum = 0;
                for (size_t j = start; j < end; ++j) {
                    sum += j * j;
                }
                return sum;
            }));
        }
        
        long long total = 0;
        for (auto& future : futures) {
            total += future.get();
        }
        
        std::cout << "parallel sum of squares: " << total << "\n";
    });
    
    auto sequential_time = measure_time([&]() {
        long long sum = 0;
        for (size_t i = 0; i < N; ++i) {
            sum += i * i;
        }
        std::cout << "sequential sum of squares: " << sum << "\n";
    });
    
    std::cout << "parallel time: " << parallel_time << " ms\n";
    std::cout << "sequential time: " << sequential_time << " ms\n";
    std::cout << "speedup: " << sequential_time / parallel_time << "x\n";
}

void example_work_stealing() {
    std::cout << "\nexample 5: work steal demo\n";
    
    ThreadPool pool(4);
    
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.submit([i]() {
            std::this_thread::sleep_for(std::chrono::microseconds(i * 10));
            return i;
        }));
    }
    
    for (auto& future : futures) {
        future.get();
    }
    
    auto stats = pool.get_stats();
    std::cout << "tasks completed: " << stats.tasks_completed << "\n";
    std::cout << "tasks stolen: " << stats.tasks_stolen << "\n";
    std::cout << "work stealing efficiency: " 
              << (100.0 * stats.tasks_stolen / stats.tasks_completed) << "%\n";
}

void benchmark_vs_async() {
    std::cout << "\nThreadPool vs std::async\n";
    
    const int num_tasks = 10000;
    const int num_threads = std::thread::hardware_concurrency();
    
    auto pool_time = measure_time([&]() {
        ThreadPool pool(num_threads);
        std::vector<std::future<int>> futures;
        
        for (int i = 0; i < num_tasks; ++i) {
            futures.push_back(pool.submit([i]() {
                return i * i;
            }));
        }
        
        for (auto& future : futures) {
            future.get();
        }
    });
    
    auto async_time = measure_time([&]() {
        std::vector<std::future<int>> futures;
        
        for (int i = 0; i < num_tasks; ++i) {
            futures.push_back(std::async(std::launch::async, [i]() {
                return i * i;
            }));
        }
        
        for (auto& future : futures) {
            future.get();
        }
    });
    
    std::cout << "ThreadPool time: " << pool_time << " ms\n";
    std::cout << "std::async time: " << async_time << " ms\n";
    std::cout << "ThreadPool is " << async_time / pool_time << "x faster\n";
}

void benchmark_throughput() {
    std::cout << "\nthroughput test\n";
    
    const int num_tasks = 1000000;
    const int num_threads = std::thread::hardware_concurrency();
    
    ThreadPool pool(num_threads);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::future<void>> futures;
    futures.reserve(num_tasks);
    
    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(pool.submit([]() {
            volatile int x = 0;
            x = x + 1;
        }));
    }
    
    for (auto& future : futures) {
        future.get();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    
    std::cout << "total tasks: " << num_tasks << "\n";
    std::cout << "time: " << elapsed << " seconds\n";
    std::cout << "throughput: " << (num_tasks / elapsed) << " tasks/sec\n";
    
    auto stats = pool.get_stats();
    std::cout << "tasks stolen: " << stats.tasks_stolen << " ("
              << (100.0 * stats.tasks_stolen / stats.tasks_completed) << "%)\n";
}

void benchmark_load_balancing() {
    std::cout << "\nload balancing\n";
    
    const int num_threads = 4;
    ThreadPool pool(num_threads);
    
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.submit([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }));
    }
    
    for (int i = 0; i < 100; ++i) {
        futures.push_back(pool.submit([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }));
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (auto& future : futures) {
        future.get();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
    
    std::cout << "time with work stealing: " << elapsed << " ms\n";
    
    auto stats = pool.get_stats();
    std::cout << "work stealing helped balance load - " 
              << stats.tasks_stolen << " tasks stolen\n";
}

void example_shutdown() {
    std::cout << "\nshutdown examples\n";
    
    {
        std::cout << "testing graceful shutdown\n";
        ThreadPool pool(2);
        
        for (int i = 0; i < 5; ++i) {
            pool.submit([i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::cout << "task " << i << " completed\n";
            });
        }
        
        pool.shutdown_graceful();
        std::cout << "all tasks completed before shutdown\n";
    }
    
    {
        std::cout << "\ntesting immediate shutdown\n";
        ThreadPool pool(2);
        
        for (int i = 0; i < 10; ++i) {
            pool.submit([i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::cout << "task " << i << " completed\n";
            });
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        pool.shutdown_immediate();
        std::cout << "shutdown immediately, note some tasks may not complete\n";
    }
}

int main() {
    std::cout << "THREAD POOL EXAMPLES";
    
    try {
        example_basic();
        example_priority();
        example_exceptions();
        example_parallel_computation();
        example_work_stealing();
        example_shutdown();
        
        std::cout << "\n\nBENCHMARKS\n";
        
        benchmark_vs_async();
        benchmark_throughput();
        benchmark_load_balancing();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "\nAll tests/benchmarks completed\n";
    
    return 0;
}
