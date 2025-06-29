// RedisStock.cpp
#include "RedisStock.h"

RedisStock::RedisStock(const std::string &redisURI) 
    : redis(redisURI) {}

bool RedisStock::lockStock(const std::string &lockKey, int timeout_ms) {
    // 使用 SET NX PX 实现分布式锁
    return redis.set(lockKey, "1", std::chrono::milliseconds(timeout_ms), sw::redis::UpdateType::NOT_EXIST);
}

bool RedisStock::unlockStock(const std::string &lockKey) {
    // 释放锁：简单 DEL（应保证只删除自己的锁，实际可用脚本）
    return redis.del(lockKey) > 0;
}

bool RedisStock::decrementStock(const std::string &stockKey) {
    // 原子扣减库存
    auto newStock = redis.decr(stockKey);
    return (newStock >= 0);
}
