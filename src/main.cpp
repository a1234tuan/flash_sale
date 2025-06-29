#include <iostream>
#include <thread>
#include "bloom_filter/BloomFilter.h"
#include "rate_limiter/RateLimiter.h"
#include "user_elig/UserLottery.h"
#include "order/OrderStatus.h"
#include "redis_ctrl/RedisStock.h"
#include "logger/Logger.h"

int main() {
    // 初始化 Logger
    Logger::init("logs/flashsale.log");
    auto lg = Logger::getLogger();
    spdlog::info("秒杀系统启动");

    // 初始化 Redis 连接
    RedisStock redisStock("tcp://127.0.0.1:6379");
    // 预设库存Key和初始库存
    std::string stockKey = "product_stock";
    redisStock.redis.set(stockKey, "1000"); // 初始1000件商品

    // 初始化限流器：1秒最多1000次
    RateLimiter rateLimiter(1, 1000);

    // 初始化 Bloom 过滤器，假设容量10000、5个哈希函数
    BloomFilter bloom(10000, 5);
    // 模拟加入一些已知非法用户
    bloom.add("malicious_user");

    // 初始化用户去重+抽签模块，选取10名中签用户
    UserLottery lottery(10);

    // 模拟100,000个并发请求
    const int NUM_REQUESTS = 100000;
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_REQUESTS; ++i) {
        threads.emplace_back([i, &bloom, &rateLimiter, &lottery, &redisStock, &stockKey]() {
            int userId = i; // 模拟用户ID
            // 布隆过滤：过滤恶意请求
            if (bloom.contains(std::to_string(userId))) {
                spdlog::warn("用户 {} 请求被布隆过滤拦截", userId);
                return;
            }
            // 限流判断
            if (!rateLimiter.allow()) {
                spdlog::warn("用户 {} 请求被限流", userId);
                return;
            }
            // 用户参与登记（去重+加入候选）
            lottery.registerUser(userId);
            // 尝试扣减库存（原子操作）
            if (redisStock.decrementStock(stockKey)) {
                spdlog::info("用户 {} 成功抢购，库存扣减", userId);
            } else {
                spdlog::info("用户 {} 抢购失败，库存不足", userId);
            }
        });
    }
    for (auto &t : threads) t.join();

    // 抽签选出中签用户
    lottery.selectWinners();
    auto winners = lottery.getWinners();
    for (int uid : winners) {
        spdlog::info("用户 {} 中签", uid);
    }

    // 统计结果
    spdlog::info("秒杀结束，共成功 {} 人抢到货", (int)winners.size());
    return 0;
}
