/**
 * @file main.cpp
 * @author Ying.Wang (373080679@qq.com)
 * @brief 实现一个线程池，可根据实际情况，自由设置线程池中线程个数
 * @version 0.1
 * @date 2024-11-11
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <iostream>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <unistd.h>

class ThreadPool {
public:
    ThreadPool(int numThreads = 10) : stop(false) {
        numThreads = std::max(1, std::min(numThreads, 100));

        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        condition.wait(lock, [this]() { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) {
                            return;
                        }
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex);
            stop = true;
        }
        condition.notify_all();
        for (auto& thread : threads) {
            thread.join();
        }
    }

    template <typename Func, typename... Args>
    void enqueue(Func&& func, Args&&... args) {
        std::function<void()> task = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
        {
            std::unique_lock<std::mutex> lock(mutex);
            tasks.emplace(task);
        }
        condition.notify_one();
    }

    void setNumThreads(int numThreads) {
        std::unique_lock<std::mutex> lock(mutex);
        numThreads = std::max(1, std::min(numThreads, 100));

        if (numThreads > threads.size()) {
            for (int i = threads.size(); i < numThreads; ++i) {
                threads.emplace_back([this]() {
                    while (true) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(mutex);
                            condition.wait(lock, [this]() { return stop || !tasks.empty(); });
                            if (stop && tasks.empty()) {
                                return;
                            }
                            task = std::move(tasks.front());
                            tasks.pop();
                        }
                        task();
                    }
                });
            }
        } else if (numThreads < threads.size()) {
            stop = true;
            lock.unlock();
            condition.notify_all();
            for (auto& thread : threads) {
                thread.join();
            }
            threads.erase(threads.begin() + numThreads, threads.end());
            stop = false;
            for (auto& thread : threads) {
                thread = std::thread([this]() {
                    while (true) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(mutex);
                            condition.wait(lock, [this]() { return stop || !tasks.empty(); });
                            if (stop && tasks.empty()) {
                                return;
                            }
                            task = std::move(tasks.front());
                            tasks.pop();
                        }
                        task();
                    }
                });
            }
        }
    }

private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::mutex mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
};

int main() {
    ThreadPool pool(10);

    for (int i = 0; i < 10; ++i) {
        pool.enqueue([](int num) {
            std::cout << "Task " << num << " executing in thread " << std::this_thread::get_id() << std::endl;
        }, i);
    }

    pool.setNumThreads(20);

    for (int i = 10; i < 20; ++i) {
        pool.enqueue([](int num) {
            std::cout << "Task " << num << " executing in thread " << std::this_thread::get_id() << std::endl;
        }, i);
    }

    return 0;
}
