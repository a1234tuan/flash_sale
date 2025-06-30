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
    std::string stockKey = "product_stock";
    redisStock.redis.set(stockKey, "1000"); // 初始1000件商品
    std::string lockKey = "lock_" + stockKey;

    // 初始化限流器：1秒最多1000次
    RateLimiter rateLimiter(1, 1000);

    // 初始化 Bloom 过滤器
    BloomFilter bloom(10000, 5);
    bloom.add("malicious_user");

    // 初始化用户资格控制
    UserLottery lottery(10);

    // 模拟 100,000 个并发请求
    const int NUM_REQUESTS = 100000;
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_REQUESTS; ++i) {
        threads.emplace_back([i, &bloom, &rateLimiter, &lottery, &redisStock, &stockKey, &lockKey]() {
            int userId = i;

            // 恶意用户过滤
            if (bloom.contains(std::to_string(userId))) {
                spdlog::warn("用户 {} 请求被布隆过滤拦截", userId);
                return;
            }

            // 限流控制
            if (!rateLimiter.allow()) {
                spdlog::warn("用户 {} 请求被限流", userId);
                return;
            }

            // 用户资格登记
            lottery.registerUser(userId);

            // 加锁尝试
            if (redisStock.lockStock(lockKey, 100)) { // 加锁成功
                if (redisStock.decrementStock(stockKey)) {
                    spdlog::info("用户 {} 成功抢购，库存扣减", userId);
                } else {
                    spdlog::info("用户 {} 抢购失败，库存不足", userId);
                }
                redisStock.unlockStock(lockKey); // 释放锁
            } else {
                spdlog::warn("用户 {} 抢购时未能获取锁", userId);
            }
        });
    }

    for (auto &t : threads) t.join();

    // 抽签并输出结果
    lottery.selectWinners();
    auto winners = lottery.getWinners();
    for (int uid : winners) {
        spdlog::info("用户 {} 中签", uid);
    }

    spdlog::info("秒杀结束，共成功 {} 人抢到货", (int)winners.size());
    return 0;
}
