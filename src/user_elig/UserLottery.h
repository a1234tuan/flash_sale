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
