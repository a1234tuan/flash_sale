// RateLimiter.h
#include <vector>
#include <mutex>
#include <chrono>

class RateLimiter {
public:
    RateLimiter(size_t window_size, size_t maxReqPerWindow);
    bool allow();
private:
    size_t window_size;
    size_t maxReq;
    std::vector<size_t> window;       // 环形窗口计数
    size_t head_index;                // 当前头部时间片索引
    std::chrono::steady_clock::time_point window_start;
    std::mutex mtx;
};
