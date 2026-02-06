#pragma once
#include <vector>

struct Route {
    int vehicleId;
    std::vector<int> seq;
    double cost = 0.0;
};
