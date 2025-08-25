#pragma once
#include <atomic>
#include <cmath>

extern "C" {
    #include "libavutil/time.h"
}

class Clock {
public:
    Clock() = default;
    
    void set(double pts, double time) 
    {
        pts_.store(pts);
        last_updated_.store(time);
    }
    
    void set(double pts) 
    {
        set(pts, currentTime());
    }
    
    double get() const 
    {
        double time = currentTime();
        return pts_.load() + (time - last_updated_.load());
    }
    
    double pts() const { return pts_.load(); }
    double lastUpdated() const { return last_updated_.load(); }
    
    void setPrePts(double pre_pts) { pre_pts_.store(pre_pts); }
    void setPreFrameDelay(double delay) { pre_frame_delay_.store(delay); }
    
    double getPrePts() const { return pre_pts_.load(); }
    double getPreFrameDelay() const { return pre_frame_delay_.load(); }

private:
    static double currentTime() 
    {
        // 使用系统时间，单位秒
        return static_cast<double>(av_gettime()) / 1000000.0;
    }
    
    std::atomic<double> pts_{0};          // 当前时钟值（秒）
    std::atomic<double> last_updated_{0}; // 最后更新时间（秒）
    std::atomic<double> pre_pts_{0};      // 上一帧的PTS
    std::atomic<double> pre_frame_delay_{0}; // 上一帧的延迟
};