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
