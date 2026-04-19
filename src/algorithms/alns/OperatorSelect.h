#pragma once
#include <random>
#include <vector>

inline int selectOperator(const std::vector<double>& weights, std::mt19937& rng) {
    std::discrete_distribution<> dist(weights.begin(), weights.end());
    return dist(rng);
}
