#include "DestroyOperators.h"

#include <algorithm>
#include <vector>

#include "CostFunction.h"

void randomDestroy(std::vector<Route>& sol, int q) {
    static std::mt19937 rng(42);
    int totalRemoved = 0;
    while (totalRemoved < q) {
        if (sol.empty()) break;
        int rIdx = rng() % sol.size();
        if (sol[rIdx].seq.empty()) {
            bool found = false;
            for (size_t i = 0; i < sol.size(); ++i) {
                if (!sol[i].seq.empty()) {
                    found = true;
                    break;
                }
            }
            if (!found) break;
            continue;
        }

        int i = rng() % sol[rIdx].seq.size();
        sol[rIdx].seq.erase(sol[rIdx].seq.begin() + i);
        sol[rIdx].isDirty = true;
        totalRemoved++;
    }
}

void LongestRouteTailRemoval(std::vector<Route>& sol, int q) {
    for (int k = 0; k < q; ++k) {
        double maxCost = -1;
        int worstRouteIdx = -1;
        int longestRoute = -1;
        size_t maxLen = 0;
        for (size_t i = 0; i < sol.size(); ++i) {
            if (sol[i].seq.size() > maxLen) {
                maxLen = sol[i].seq.size();
                longestRoute = i;
            }
        }

        if (longestRoute != -1) {
            sol[longestRoute].seq.pop_back();
            sol[longestRoute].isDirty = true;
        } else {
            break;
        }
    }
}

void vehicleDestroy(std::vector<Route>& sol) {
    int worst = -1;
    size_t minLoad = 100000;
    for (size_t i = 0; i < sol.size(); i++) {
        if (!sol[i].seq.empty() && sol[i].seq.size() < minLoad) {
            minLoad = sol[i].seq.size();
            worst = i;
        }
    }

    if (worst >= 0) {
        sol[worst].seq.clear();
        sol[worst].isDirty = true;
    }
}

void WorstRouteTailRemoval(std::vector<Route>& sol, const std::vector<Employee>& emp, const std::vector<Vehicle>& veh,
                           const Metadata& meta) {
    int q = 2;
    while (q--) {
        double maxCost = -1;
        int rIdx = -1;

        for (size_t i = 0; i < sol.size(); ++i) {
            if (sol[i].seq.empty()) continue;
            double c = routeCost(sol[i], veh[i], emp, meta);
            if (c > maxCost) {
                maxCost = c;
                rIdx = i;
            }
        }

        if (rIdx != -1) {
            sol[rIdx].seq.pop_back();
            sol[rIdx].isDirty = true;
        } else {
            break;
        }
    }
}
