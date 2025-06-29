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
