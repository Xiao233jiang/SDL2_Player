#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include "../../ffmpeg_utils/ffmpeg_headers.hpp"

template<typename T>
class SafeQueue 
{
public:
    using ItemCleanupFunc = std::function<void(T&)>;

    SafeQueue(size_t max_size = 0, ItemCleanupFunc cleanup_func = nullptr)
        : max_size_(max_size), cleanup_func_(cleanup_func) {}

    // 入队（带容量检查）
    bool push(const T& value, bool blocking = true, int timeout_ms = 100) 
    {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (max_size_ > 0 && queue_.size() >= max_size_) 
        {
            if (!blocking) 
            {
                return false; // 非阻塞模式直接返回失败
            }
            if (!cond_not_full_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                [this]() { return queue_.size() < max_size_ || quit_.load(); })) 
            {
                return false; // 超时返回失败
            }
        }
        
        if (quit_.load()) return false;
        
        queue_.push(value);
        lock.unlock();
        cond_not_empty_.notify_one();
        return true;
    }

    // 出队（阻塞）
    bool pop(T& item, std::atomic<bool>& quit, int timeout_ms = 100) 
    {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (!cond_not_empty_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
            [this, &quit]() { return !queue_.empty() || quit.load() || quit_.load(); })) 
        {
            return false;
        }
        
        if ((quit.load() || quit_.load()) && queue_.empty()) return false;
        
        item = std::move(queue_.front());
        queue_.pop();
        
        if (max_size_ > 0) 
        {
            lock.unlock();
            cond_not_full_.notify_one();
        }
        return true;
    }

    // 非阻塞出队
    bool try_pop(T& value) 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop();
        
        if (max_size_ > 0) 
        {
            cond_not_full_.notify_one();
        }
        return true;
    }

    // 队列大小
    size_t size() const 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    // 是否为空
    bool empty() const 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    // 清空队列
    void clear() 
    {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) 
        {
            T item = std::move(queue_.front());
            if (cleanup_func_) 
            {
                cleanup_func_(item);
            } 
            else 
            {
                default_cleanup(item);
            }
            queue_.pop();
        }
        cond_not_full_.notify_all();
    }

    // 设置退出标志
    void set_quit(bool quit = true) 
    {
        quit_.store(quit);
        if (quit) 
        {
            cond_not_empty_.notify_all();
            cond_not_full_.notify_all();
        }
    }

    // 检查是否设置了退出标志
    bool is_quit() const {
        return quit_.load();
    }

    // 添加front方法 - 查看队列头部元素但不移除
    bool front(T& item) const {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = queue_.front();
        return true;
    }

    // 获取最大容量
    size_t max_size() const { return max_size_; }

    // 新增：重置队列状态（用于重新加载文件）
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        // 先清空队列
        while (!queue_.empty()) 
        {
            T item = std::move(queue_.front());
            if (cleanup_func_) 
            {
                cleanup_func_(item);
            } 
            else 
            {
                default_cleanup(item);
            }
            queue_.pop();
        }
        
        // 重置退出标志
        quit_.store(false);
        
        // 通知等待的线程
        cond_not_empty_.notify_all();
        cond_not_full_.notify_all();
    }

    // 新增：获取统计信息
    struct QueueStats {
        size_t current_size;
        size_t max_size;
        bool is_quit;
    };
    
    QueueStats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {queue_.size(), max_size_, quit_.load()};
    }

private:
    // 默认清理函数
    void default_cleanup(T& item) 
    {
        if constexpr (std::is_same_v<T, AVPacket>) 
        {
            av_packet_unref(&item);
        } 
        else if constexpr (std::is_same_v<T, AVFrame*>) 
        {
            if (item) av_frame_free(&item);
        }
        // 其他类型不需要特殊清理
    }

    mutable std::mutex mutex_;
    std::condition_variable cond_not_empty_;
    std::condition_variable cond_not_full_;
    std::queue<T> queue_;
    size_t max_size_ = 0;
    ItemCleanupFunc cleanup_func_ = nullptr;
    std::atomic<bool> quit_{false};
};