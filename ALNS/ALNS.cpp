#include "ALNS.h"
#include "DestroyOperators.h"
#include "RepairOperators.h"
#include "CostFunction.h"
#include "OperatorStats.h"
#include "OperatorSelect.h"
#include<map>
#include <cmath>
#include <random>
#include <vector>
#include <algorithm>
#include <iostream>

// Helper struct for Population
struct SolutionState {
    std::vector<Route> sol;
    double cost;
};

std::vector<Route> solveALNS(
    const std::vector<Employee>& emp,
    const std::vector<Vehicle>& veh,
    const Metadata& meta)
{
    std::mt19937 rng(7);

    // ------------------ Initial solution ------------------
    std::vector<Route> initialSol;
    for (const auto& v : veh)
        initialSol.push_back({v.id, {}});

    greedyRepair(initialSol, emp, veh, meta);

    double initialCost = 0.0;
    for (const auto& r : initialSol)
        initialCost += routeCost(r, veh[r.vehicleId], emp, meta);

    // ------------------ Initialize Pool ------------------
    const int POOL_SIZE = 5;
    std::vector<SolutionState> pool;
    
    // Fill pool with clones of initial solution
    for(int i=0; i<POOL_SIZE; ++i) {
        pool.push_back({initialSol, initialCost});
    }

    SolutionState bestGlobal = pool[0];

    // ------------------ ALNS parameters ------------------
    
    enum DestroyOp { RAND_D, WORST_D, VEHICLE_D, TIMEWIN_D };
    enum RepairOp  { GREEDY_R, RANDOM_R, REGRET_R };

    std::vector<OperatorStats> dStats(4);
    std::vector<OperatorStats> rStats(3);
    
    double T = 1000.0;

    // ------------------ Main loop ------------------
    int tot_it =50000;
    for (int it = 1; it < tot_it; it++) {
    
        int pIdx = std::uniform_int_distribution<>(0, POOL_SIZE - 1)(rng);
        SolutionState cur = pool[pIdx];
        auto next = cur.sol;

        
        int d = selectOperator(
            { dStats[0].weight, dStats[1].weight,
              dStats[2].weight, dStats[3].weight }, rng);

        int r = selectOperator(
            { rStats[0].weight, rStats[1].weight,
              rStats[2].weight }, rng);
        
        double k1=0.10;
              if(it<(int)(0.04* tot_it)) k1=0.20;
              else if(it<(int)(0.15*tot_it)) k1=0.15;
              else if(it<(int)(0.9*tot_it)) k1=0.1;
              else k1=0.05;
        int k2 = it<(tot_it/1.01) ? 2 : 1;
        int q = std::max(k2, static_cast<int>(k1 * emp.size()));
        if (d == RAND_D) randomDestroy(next,q );    
        else if (d == WORST_D) worstCostDestroy(next, q);
        else if (d == VEHICLE_D) vehicleDestroy(next);
        else timeWindowDestroy(next, emp, veh, meta);


        if (r == GREEDY_R) greedyRepair(next, emp, veh, meta);
        else if (r == RANDOM_R) randomRepair(next, emp, veh, meta);
        else regretRepair(next, emp, veh, meta, q); 


        double nextCost = 0.0;
        for (const auto& rt : next)
            nextCost += routeCost(rt, veh[rt.vehicleId], emp, meta);

        // ----- Reward computation -----
        double reward = 0.0;
        
 
        if (nextCost < bestGlobal.cost) {
            bestGlobal = {next, nextCost};
            reward = 5.0;
        } 
        else if (nextCost < cur.cost) {
            reward = 2.0;
        }
        else {
            reward = 0.5;
        }

        // ----- Pool Update Logic -----

        int worstIdx = -1;
        double worstCost = -1.0;
        for(int i=0; i<POOL_SIZE; ++i) {
            if (pool[i].cost > worstCost) {
                worstCost = pool[i].cost;
                worstIdx = i;
            }
        }
        
        bool accept = false;
        

        if (nextCost < worstCost) {
            accept = true;
            pool[worstIdx] = {next, nextCost};
        } else {

             double diff = nextCost - cur.cost; // compare to parent
             if (T > 1e-6) {
                 double p = std::exp(-diff / T);
                 if (p > std::uniform_real_distribution<>(0.0, 1.0)(rng)) {

                     pool[pIdx] = {next, nextCost};
                     accept = true;
                 }
             }
        }


        dStats[d].score += reward;
        rStats[r].score += reward;
        dStats[d].uses++;
        rStats[r].uses++;

        // ----- Cooling -----
        T *= 0.999;

        if (it % 100 == 0) {
            for (auto& o : dStats) {
                if (o.uses > 0) {
                    o.weight = 0.8 * o.weight + 0.2 * (o.score / o.uses);
                    o.score = 0; 
                    o.uses = 0; 
                }
            }

            for (auto& o : rStats) {
                if (o.uses > 0) {
                    o.weight = 0.8 * o.weight + 0.2 * (o.score / o.uses);
                    o.score = 0;
                    o.uses = 0;
                }
            }
        }
    }

    return bestGlobal.sol;
}

