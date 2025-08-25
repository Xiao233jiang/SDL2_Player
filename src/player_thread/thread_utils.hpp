#pragma once
#include <mutex>
#include <iostream>
#include <atomic>
#include <thread>

extern std::mutex g_cout_mutex;

#define THREAD_SAFE_COUT(x) do { \
    std::lock_guard<std::mutex> lock(g_cout_mutex); \
    std::cout << x << std::endl; \
} while (0)

class ThreadBase 
{
public:
    virtual ~ThreadBase() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void join() = 0;
    
protected:
    std::thread thread_;
    std::atomic<bool> running_{false};
};