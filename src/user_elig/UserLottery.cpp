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
