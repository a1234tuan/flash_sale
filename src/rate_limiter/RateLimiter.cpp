// RateLimiter.cpp
#include "RateLimiter.h"

RateLimiter::RateLimiter(size_t window_size, size_t maxReqPerWindow)
    : window_size(window_size), maxReq(maxReqPerWindow),
      window(window_size, 0), head_index(0) {
    window_start = std::chrono::steady_clock::now();
}

bool RateLimiter::allow() {
    std::lock_guard<std::mutex> lock(mtx);
    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - window_start).count();
    size_t new_index = (head_index + elapsed_ms / 1000) % window_size;

    // 如果时间窗口前移，更新环形数组：清零过时窗格
    if (elapsed_ms >= 1000) {
        size_t steps = (elapsed_ms / 1000);
        for (size_t i = 1; i <= steps && i <= window_size; ++i) {
            window[(head_index + i) % window_size] = 0;
        }
        head_index = new_index;
        window_start = now;
    }

    // 计算当前滑动窗口内请求总数
    size_t total = 0;
    for (auto count : window) total += count;
    if (total >= maxReq) {
        return false; // 超过阈值，拒绝请求
    }
    // 记录当前请求
    window[head_index]++;
    return true;
}
