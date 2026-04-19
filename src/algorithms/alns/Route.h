#pragma once
#include <vector>

struct Route {
    int vehicleId;
    std::vector<int> seq;
    mutable double cachedCost = 0.0;
    mutable bool isDirty = true;
};
