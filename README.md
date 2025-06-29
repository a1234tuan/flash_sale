# flash_sale

电商秒杀系统核心模块C+++redis

问题描述：设计高并发下商品抢购系统，解决超卖与恶意请求问题。

数据结构核心应用：

 布隆过滤器（恶意请求拦截）

 循环队列（请求限流）

 链表（订单状态机管理）

基本要求：

✅ Redis 模拟库存原子操作（DECR 命令）

✅ 请求队列控制并发（每秒处理上限）

✅ 用户资格验证（哈希去重+随机抽签）

✅ 订单状态流转（创建→支付→完成）

测试数据：5 类商品（库存 1000/类）、10 万用户请求日志

扩展要求：Redis 集群分布式锁、自动熔断机制

项目描述
电商秒杀系统核心模块
问题描述：设计高并发下商品抢购系统，解决超卖与恶意请求问题。
数据结构核心应用：
· 布隆过滤器（恶意请求拦截） 
· 循环队列（请求限流） 
· 链表（订单状态机管理） 
基本要求：
✅ Redis 模拟库存原子操作（DECR 命令）
✅ 请求队列控制并发（每秒处理上限） 
✅ 用户资格验证（哈希去重+随机抽签） 
✅ 订单状态流转（创建→支付→完成） 
测试数据：5 类商品（库存 1000/类）、10 万用户请求日志

项目文件
目录结构：
flash_sale/            # 项目根目录
├── CMakeLists.txt     # CMake构建配置
├── src/               # 源码目录
│   ├── bloom_filter/  # 布隆过滤器模块
│   │   ├── BloomFilter.h
│   │   └── BloomFilter.cpp
│   ├── rate_limiter/  # 限流模块
│   │   ├── RateLimiter.h
│   │   └── RateLimiter.cpp
│   ├── user_elig/     # 用户资格验证模块（去重+抽签）
│   │   ├── UserLottery.h
│   │   └── UserLottery.cpp
│   ├── order/         # 订单状态机模块
│   │   ├── OrderStatus.h
│   │   └── OrderStatus.cpp
│   ├── redis_ctrl/    # Redis库存控制模块
│   │   ├── RedisStock.h
│   │   └── RedisStock.cpp
│   ├── logger/        # 日志模块（spdlog）
│   │   ├── Logger.h
│   │   └── Logger.cpp
│   └── main.cpp       # 程序入口，模拟并发请求
└── logs/              # 日志输出目录（运行后生成）
布隆过滤器模块 （BloomFilter）
功能说明： 布隆过滤器是一种空间高效的概率性数据结构，用于判断某个元素是否在集合中。在秒杀系统中，可用来快速过滤恶意或重复请求，例如将已知非法用户或重复请求加入布隆过滤器。对合法用户请求，只在布隆过滤器中查询；若不存在，则视为可能合法，继续处理；若存在，则可能为重复或恶意（存在一定误判率），可拒绝或二次验证
// BloomFilter.cpp
#include "BloomFilter.h"
#include <string>
#include <functional>

BloomFilter::BloomFilter(size_t bits, size_t hashCount)
    : bitset(bits), hashCount(hashCount) {}

void BloomFilter::add(const std::string &key) {
    for (size_t i = 0; i < hashCount; ++i) {
        size_t hash = std::hash<std::string>{}(std::to_string(i) + key);
        bitset[hash % bitset.size()] = true;
    }
}

bool BloomFilter::contains(const std::string &key) const {
    for (size_t i = 0; i < hashCount; ++i) {
        size_t hash = std::hash<std::string>{}(std::to_string(i) + key);
        if (!bitset[hash % bitset.size()]) {
            return false;
        }
    }
    return true;
}
// BloomFilter.h
#include <vector>
#include <functional>
#include <bitset>

class BloomFilter {
public:
    BloomFilter(size_t bits, size_t hashCount);
    void add(const std::string &key);
    bool contains(const std::string &key) const;
private:
    std::vector<bool> bitset;
    size_t hashCount;
    // 多个哈希函数，这里简单使用std::hash变体
    std::vector<std::hash<std::string>> hashFuncs;
};
调试分析
· 测试数据：加入 malicious_user，模拟判断是否拦截
· 时间复杂度：O(k) 判断 / 插入时间
· 问题：初期未控制内存大小，导致 vector 太大，后改为固定位宽
· 改进设想：未来可引入分布式布隆过滤器或动态调整位图长度
限流模块 （RateLimiter）
功能说明： 限流模块采用循环队列或滑动窗口算法，实现“每秒最多 N 次请求”限制。常用的方法是固定时间窗口或滑动时间窗口。这里可将时间分为若干个小片段（如1秒内按毫秒划分），用大小固定的环形数组记录每个小片段的请求数。当有新请求时，将当前时间定位到对应小窗口，计算过去1秒内所有小窗口的总请求数。如果超过阈值，则拒绝请求；否则允许并将当前窗口计数加1
关键代码示例： 基于环形数组的滑动窗口限流（示例每秒最多 maxReqPerSec 次）：
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
 上述逻辑使用固定长度的环形数组记录最近1秒内的请求总量。blog.csdn.net指出滑动窗口算法可通过环形数组来统计流量，避免频繁重置。此处每次计算环形窗口总和，与阈值比较，以决定是否允许当前请求。  
● 调试分析
· 时间复杂度：均摊 O(1) 插入/移除
· 问题：初期没考虑系统时间戳重复，后加入 getCurrentTimeMillis() 函数
用户资格验证模块 （UserLottery）
功能说明： 用户资格验证模块用于在海量并发请求中去重并随机抽签决定最终获赠用户。具体做法是：维护一个哈希集合（std::unordered_set）记录已参与过秒杀的用户ID，实现去重；之后将所有合法的（且未重复的）用户放入列表，利用随机数或打乱算法（如 Fisher–Yates shuffle）从中抽取中奖用户列表。这样保证每个用户只有一次机会，并随机选出中奖者。
// UserLottery.cpp
#include "UserLottery.h"
#include <algorithm> // Required for std::shuffle

UserLottery::UserLottery(size_t numWinners) : numWinners(numWinners) {}

void UserLottery::registerUser(int userId) {
    // 去重：只有新用户才添加到候选列表
    if (userSet.insert(userId).second) {
        candidates.push_back(userId);
    }
}

void UserLottery::selectWinners() {
    // 随机打乱候选列表
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(candidates.begin(), candidates.end(), g);
    // 选取前 numWinners 个用户为胜者
    for (size_t i = 0; i < numWinners && i < candidates.size(); ++i) {
        winners.push_back(candidates[i]);
    }
}

const std::vector<int>& UserLottery::getWinners() const {
    return winners;
}
// UserLottery.h
#include <unordered_set>
#include <vector>
#include <random>

class UserLottery {
public:
    UserLottery(size_t numWinners);
    void registerUser(int userId);
    void selectWinners();
    const std::vector<int>& getWinners() const;
private:
    std::unordered_set<int> userSet;  // 已登记用户集合，用于去重
    std::vector<int> candidates;      // 候选用户列表
    std::vector<int> winners;
    size_t numWinners;
};
 说明：registerUser 用于记录每个请求的用户 ID，通过 unordered_set 实现哈希去重。所有唯一的用户ID存入 candidates。最后 selectWinners() 随机打乱候选列表后选出前 N 名（可作为中奖用户）。可使用 std::shuffle（C++11起）和 std::mt19937 产生公平的随机顺序。  
● 调试分析
· 问题：早期 shuffle 函数不可用，后添加 #include <algorithm> 解决
· 思考：可扩展为预签名请求与资格列表结合使用
订单状态机模块 （OrderStatus）
功能说明： 订单状态机管理订单在不同阶段（创建、支付、完成）之间的状态转换。可以使用链表或状态模式来实现：例如将每个状态封装为节点，节点按顺序链表相连；订单持有一个指向当前状态节点的指针。每次触发操作时，将指针移动到下一个节点，表示状态进程。目前状态节点包括 Created、Paid、Completed。
关键代码示例： 下面示例将状态作为枚举，并使用链表链接状态节点。每个状态节点指向下一个可能状态。
// OrderStatus.cpp
#include "OrderStatus.h"

OrderStatus::OrderStatus() {
    // 构造链表：Created -> Paid -> Completed
    head = new StateNode(OrderState::Created);
    head->next = new StateNode(OrderState::Paid);
    head->next->next = new StateNode(OrderState::Completed);
    cur = head;
}

OrderStatus::~OrderStatus() {
    // 释放链表节点
    StateNode* ptr = head;
    while(ptr) {
        StateNode* next = ptr->next;
        delete ptr;
        ptr = next;
    }
}

OrderState OrderStatus::getState() const {
    return cur->state;
}

void OrderStatus::nextState() {
    if (cur->next) {
        cur = cur->next;
    }
}
// OrderStatus.h
#include <string>

enum class OrderState { Created, Paid, Completed };

struct StateNode {
    OrderState state;
    StateNode* next;
    StateNode(OrderState s) : state(s), next(nullptr) {}
};

class OrderStatus {
public:
    OrderStatus();
    ~OrderStatus();
    OrderState getState() const;
    void nextState();  // 推动订单到下一个状态
private:
    StateNode* head;   // 状态链表头（Created）
    StateNode* cur;    // 当前状态节点
};
在业务流程中，订单创建时状态为 Created；用户付款成功后调用 nextState() 将状态推进到 Paid；最后系统确认完成时再次 nextState() 到 Completed。使用链表结构简洁地表示了有序状态转换。

Redis库存控制模块（RedisStock）
功能说明： 使用 redis-plus-plus C++ 客户端与 Redis 交互，执行库存管理（原子扣减库存）、分布式锁等。秒杀时关键操作为库存减一（DECR），Redis 的 DECR 命令是原子操作，可保证多客户端环境中安全扣减w3resource.com。还可借助 Redis 实现分布式锁（例如Redlock算法）。redis-plus-plus 自带对 Redlock 的支持，提供 RedMutex 类来方便地锁定资源github.comgithub.com。
关键代码示例： 下面展示如何使用 redis-plus-plus 执行库存扣减和加锁。
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
调试分析
· 问题：MinGW 无法正常链接 hiredis，后更换为 MSVC 编译器
· 改进设想：库存可迁移至内存缓存，使用双写同步
日志模块（Logger，使用 spdlog）
功能说明： 使用 spdlog 记录系统运行日志，包括用户请求、限流/拦截结果、订单状态变更、库存变动、错误信息等。spdlog 是高性能的 C++ 日志库，支持线程安全、格式化、文件轮转等功能github.comgithub.com。可创建全局日志器，在各模块中记录信息。
// Logger.cpp
#include "Logger.h"

std::shared_ptr<spdlog::logger> Logger::logger;

void Logger::init(const std::string& logFile) {
    // 初始化基本的文件日志器
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
    logger = spdlog::basic_logger_mt("flashsale", logFile);
    spdlog::set_default_logger(logger);
}

std::shared_ptr<spdlog::logger> Logger::getLogger() {
    return logger;
}
// Logger.h
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

class Logger {
public:
    static void init(const std::string& logFile);
    static std::shared_ptr<spdlog::logger> getLogger();
private:
    static std::shared_ptr<spdlog::logger> logger;
};
● 调试分析
· 问题：初期未安装 spdlog，配置 CMake 和 vcpkg 后成功解决
 主程序（main.cpp）示例  
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
● 调试分析
· 多线程稳定性较好，日志量大时建议使用异步日志
CMake构建配置
cmake_minimum_required(VERSION 3.10)
project(FlashSale)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 引入 vcpkg 工具链
# 请确保你在命令行执行时加上 -DCMAKE_TOOLCHAIN_FILE=...（看底部说明）

# 查找依赖
find_package(Threads REQUIRED)
find_package(redis++ CONFIG REQUIRED)   # 自动包含 hiredis
find_package(spdlog CONFIG REQUIRED)

# 包含头文件路径（包括当前 src 目录）
include_directories(
    ${PROJECT_SOURCE_DIR}/src
)

# 递归查找 src 下所有 .cpp 文件
file(GLOB_RECURSE SRC
    src/*.cpp
)

# 生成可执行文件
add_executable(flash_sale ${SRC})

# 链接依赖库
target_link_libraries(flash_sale
    Threads::Threads
    redis++::redis++     # ✅ redis++ 会自动链接 hiredis
    spdlog::spdlog       # ✅ spdlog 日志库
)

# 包含 src 作为 include 目录（用于头文件 include 相对路径）
target_include_directories(flash_sale PRIVATE ${PROJECT_SOURCE_DIR}/src)
日志示例
示例日志内容：
makefile


复制编辑
12:00:00 [info] 秒杀系统启动
12:00:00 [warn] 用户 5 请求被限流
12:00:00 [info] 用户 42 成功抢购，库存扣减
12:00:00 [info] 用户 43 成功抢购，库存扣减
...
12:00:01 [info] 用户 7 中签
12:00:01 [info] 用户 15 中签
...
12:00:01 [info] 秒杀结束，共成功 10 人抢到货
以上日志显示了用户请求被拦截或限流的情况，以及抢购成功与中签情况的记录。通过 spdlog 的格式化，可以方便定位每条日志的级别和线程信息。总结统计成功人数即为模拟的秒杀结果。
引用： 本设计参考了相关资料的实现思路：Bloom 过滤器可快速拦截请求en.wikipedia.orgen.wikipedia.org；滑动窗口限流可用环形数组实现blog.csdn.net；redis-plus-plus 提供了原子命令接口以及 Redlock 支持github.comstackoverflow.com；spdlog 库用于高效日志记录github.comgithub.com。以上模块协同工作，实现了一个模块化、可扩展的秒杀系统框架。
项目运行说明
前提：你已打开 VSCode 的终端（PowerShell）并定位到你的项目目录：
cd D:/MyProjects/flash_sale

🧱 第一步：构建项目（使用 CMake + vcpkg + MSVC）
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake
说明词（你可以这样讲）：
这一行命令是用 CMake 生成项目的构建文件，使用 vcpkg 管理的依赖库如 redis++、spdlog，并指定 MSVC 编译器环境。

🔨 第二步：开始编译项目
cmake --build build
说明词：
这一行命令会调用 MSVC 工具链，将我的所有 C++ 模块编译成一个可执行文件 flash_sale.exe，可以在 build\Debug 目录下看到编译结果。

🚀 第三步：运行项目
.\build\Debug\flash_sale.exe
说明词：
执行这行命令之后，程序会自动启动秒杀流程，模拟 10 万用户发起抢购请求，同时进行布隆过滤、限流、资格抽签、库存扣减等处理。

📄 第四步：查看日志文件内容
notepad logs/flashsale.log
说明词：
程序的所有运行日志都会写入到 logs/flashsale.log 文件中，比如：哪些用户抢购成功、哪些限流、哪些被拦截或中签，可以用于后续分析。

项目相关配置工具
VSCode + MSVC	手动运行vcvars64.bat 激活环境	推荐在 PowerShell 中执行
vcpkg	安装路径需添加到CMakeLists.txt 和.vscode	用 toolchain 接入最稳定
CMake	使用target_link_libraries + find_package	避免手动加 include 和 .lib 文件
.vscode/c_cpp_properties.json	使用 MSVC 路径 + vcpkg 头文件	保证 IntelliSense 与实际编译一致
build/ 目录	MSVC 默认输出在 build/Debug/	可用Get-ChildItem 查找.exe
