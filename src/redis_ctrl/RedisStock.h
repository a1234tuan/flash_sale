// RedisStock.h
#include <sw/redis++/redis++.h>
using namespace sw::redis;

class RedisStock {
public:
    RedisStock(const std::string &redisURI);
    bool lockStock(const std::string &lockKey, int timeout_ms);
    bool unlockStock(const std::string &lockKey);
    bool decrementStock(const std::string &stockKey);
    sw::redis::Redis redis;
};
