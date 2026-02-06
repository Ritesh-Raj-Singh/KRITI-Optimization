#include "ALNS.h"
#include "DestroyOperators.h"
#include "RepairOperators.h"
#include "CostFunction.h"
#include "OperatorStats.h"
#include "OperatorSelect.h"

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
    const std::vector<Vehicle>& veh)
{
    std::mt19937 rng(7);

    // ------------------ Initial solution ------------------
    std::vector<Route> initialSol;
    for (const auto& v : veh)
        initialSol.push_back({v.id, {}});

    greedyRepair(initialSol, emp, veh);

    double initialCost = 0.0;
    for (const auto& r : initialSol)
        initialCost += routeCost(r, veh[r.vehicleId], emp);

    // ------------------ Initialize Pool ------------------
    const int POOL_SIZE = 5;
    std::vector<SolutionState> pool;
    
    // Fill pool with clones of initial solution
    // (Ideally we would diversify, but ALNS will diverge them quickly)
    for(int i=0; i<POOL_SIZE; ++i) {
        pool.push_back({initialSol, initialCost});
    }

    SolutionState bestGlobal = pool[0];

    // ------------------ ALNS parameters ------------------
    // Temperature for SA acceptance (still use SA for "accept into pool"?)
    // Or just simple greedy acceptance into pool.
    // User requested: "store that state for some iters".
    // We'll stick to: Accept if better than Worst in Pool.
    // PLUS: Simulated Annealing to accept "bad" moves locally before checking pool?
    // Let's simplify: Standard ALNS operating on a pool member.
    // Acceptance = Update Pool.
    
    enum DestroyOp { RAND_D, WORST_D, VEHICLE_D, TIMEWIN_D };
    enum RepairOp  { GREEDY_R, RANDOM_R, REGRET_R };

    std::vector<OperatorStats> dStats(4);
    std::vector<OperatorStats> rStats(3);
    
    // T for SA is less relevant if we just rely on pool diversity, 
    // but useful for accepting moves that don't improve the *specific* parent
    // yet might improve the pool eventually.
    double T = 1000.0;

    // ------------------ Main loop ------------------
    for (int it = 0; it < 5000; it++) {
        
        // 1. Select Parent from Pool (Randomly)
        int pIdx = std::uniform_int_distribution<>(0, POOL_SIZE - 1)(rng);
        SolutionState cur = pool[pIdx]; // Copy
        auto next = cur.sol;

        // ----- Select operators (adaptive) -----
        int d = selectOperator(
            { dStats[0].weight, dStats[1].weight,
              dStats[2].weight, dStats[3].weight }, rng);

        int r = selectOperator(
            { rStats[0].weight, rStats[1].weight,
              rStats[2].weight }, rng);

        // ----- Destroy -----
        if (d == RAND_D) randomDestroy(next, 2);
        else if (d == WORST_D) worstCostDestroy(next, 2);
        else if (d == VEHICLE_D) vehicleDestroy(next);
        else timeWindowDestroy(next, emp, veh);

        // ----- Repair -----
        if (r == GREEDY_R) greedyRepair(next, emp, veh);
        else if (r == RANDOM_R) randomRepair(next, emp, veh);
        else regretRepair(next, emp, veh);

        // ----- Cost evaluation -----
        double nextCost = 0.0;
        for (const auto& rt : next)
            nextCost += routeCost(rt, veh[rt.vehicleId], emp);

        // ----- Reward computation -----
        double reward = 0.0;
        
        // Check Global Best
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
        // Find Worst in Pool
        int worstIdx = -1;
        double worstCost = -1.0;
        for(int i=0; i<POOL_SIZE; ++i) {
            if (pool[i].cost > worstCost) {
                worstCost = pool[i].cost;
                worstIdx = i;
            }
        }
        
        bool accept = false;
        
        // If better than worst, replace worst
        if (nextCost < worstCost) {
            accept = true;
            // Basic diversity check: Don't add if EXACTLY same cost as existing (cheap check)
            // (Skipped for simplicity, but recommended)
            pool[worstIdx] = {next, nextCost};
        } else {
             // SA Acceptance for "Storing" it even if worse than worst?
             // If we do that, we kill a better solution.
             // Maybe we operate on a temporary 'cur' and only push to pool if good?
             // Standard Population ALNS: Update pool if quality sufficient.
             // SA is typically used to accept `next` as the new `cur` for the *next* iteration
             // of that specific "walker".
             // Since we pick random walker each time, updating the pool IS the acceptance.
             
             // Maybe we replace Randomly if SA passes? 
             // That preserves diversity.
             double diff = nextCost - cur.cost; // compare to parent
             if (T > 1e-6) {
                 double p = std::exp(-diff / T);
                 if (p > std::uniform_real_distribution<>(0.0, 1.0)(rng)) {
                     // Accepted via SA even though not strictly better than worst?
                     // If we replace worst, we improve overall pool quality.
                     // If we replace parent, we just mutate that walker.
                     // Let's replace Parent in pool to evolve that walker.
                     pool[pIdx] = {next, nextCost};
                     accept = true;
                 }
             }
        }

        // ----- Update operator stats -----
        dStats[d].score += reward;
        rStats[r].score += reward;
        dStats[d].uses++;
        rStats[r].uses++;

        // ----- Cooling -----
        T *= 0.999;

        // ----- Adaptive weight update -----
        if (it % 50 == 0) {
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
