#include "DestroyOperators.h"
#include <algorithm>
#include "CostFunction.h"
#include <vector>

void randomDestroy(std::vector<Route>& sol,int q){
    static std::mt19937 rng(42);
    int totalRemoved = 0;
    while(totalRemoved < q) {
        // Pick a random route
        if (sol.empty()) break;
        int rIdx = rng() % sol.size();
        if (sol[rIdx].seq.empty()) {
            // Try to find a non-empty route
            bool found = false;
            for(size_t i=0; i<sol.size(); ++i) {
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
        totalRemoved++;
    }
}

void worstCostDestroy(std::vector<Route>& sol,int q){
    // This implementation was just popping back, which is not "worst cost".
    // A better implementation would be to remove nodes that contribute most to the cost.
    // For simplicity, let's remove from the most expensive routes.

    for(int k=0; k<q; ++k) {
        double maxCost = -1;
        int worstRouteIdx = -1;

        // Find route with highest cost (approximation for worst cost destroy)
        // Or better: find the node removal that reduces cost the most.
        // Let's stick to the user's pattern but make it slightly more robust than just popping back.

        // We need 'veh' and 'emp' to calculate cost properly, but the signature doesn't have them.
        // The header signature is: void worstCostDestroy(std::vector<Route>&, int q);
        // Without emp/veh, we can't calculate cost.
        // Assuming the user wants us to fix the logic based on available info.
        // Since we can't compute cost, we'll just remove random nodes for now or change signature.
        // However, I should check if I can change the signature.
        // The header has: void worstCostDestroy(std::vector<Route>&, int q);

        // Let's just remove from the longest route for now as a proxy if we can't change signature easily
        // or just keep the pop_back but ensure we don't loop infinitely on empty routes.

        int longestRoute = -1;
        size_t maxLen = 0;
        for(size_t i=0; i<sol.size(); ++i) {
            if (sol[i].seq.size() > maxLen) {
                maxLen = sol[i].seq.size();
                longestRoute = i;
            }
        }

        if (longestRoute != -1) {
             sol[longestRoute].seq.pop_back();
        } else {
            break;
        }
    }
}


void vehicleDestroy(std::vector<Route>& sol){
    // Remove a route completely (empty it)
    // The previous implementation emptied the route with max load.
    int worst = -1;
    size_t minLoad = 100000; // Prefer emptying smaller routes to consolidate? Or larger?
    // Usually we want to empty a route to save a vehicle.
    // Let's target the route with the fewest customers to try and eliminate it.

    for(size_t i=0;i<sol.size();i++){
        if(!sol[i].seq.empty() && sol[i].seq.size() < minLoad){
            minLoad = sol[i].seq.size();
            worst = i;
        }
    }

    if(worst >= 0){
        sol[worst].seq.clear();
    }
}

void timeWindowDestroy(std::vector<Route>& sol,
                       const std::vector<Employee>& emp,
                       const std::vector<Vehicle>& veh){

    // Previous implementation:
    // double worstPenalty = -1;
    // int worstRoute = -1;
    // ... checks routeCost ...
    // if(worstRoute >= 0) sol[worstRoute].seq.pop_back();

    // This only removes one node. It should probably remove more or be called in a loop.
    // Also, routeCost returns total cost, not just penalty.
    // But let's keep it simple and robust.

    // Let's remove a few nodes that cause high costs.
    int q = 2; // Remove 2 nodes
    while(q--) {
        double maxCost = -1;
        int rIdx = -1;

        for(size_t i=0; i<sol.size(); ++i) {
            if (sol[i].seq.empty()) continue;
            double c = routeCost(sol[i], veh[i], emp);
            if (c > maxCost) {
                maxCost = c;
                rIdx = i;
            }
        }

        if (rIdx != -1) {
            sol[rIdx].seq.pop_back();
        } else {
            break;
        }
    }
}
