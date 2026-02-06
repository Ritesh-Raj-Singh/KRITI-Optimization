#include "RepairOperators.h"
#include <random>
#include <algorithm>
#include "Compatibility.h"
#include "CostFunction.h"
#include "Feasibility.h"
#include <limits>

void greedyRepair(std::vector<Route>& sol,
                  const std::vector<Employee>& emp,
                  const std::vector<Vehicle>& veh){
    for(const auto& e:emp){
        bool used=false;
        for(auto& r:sol){
            if(std::find(r.seq.begin(),r.seq.end(),e.id)!=r.seq.end()){
                used=true; break;
            }
        }
        if(!used){
            double bestCost = std::numeric_limits<double>::max();
            int bestVeh = -1;

            for(size_t v=0; v<sol.size(); v++){
                // Basic compatibility checks are inside routeFeasible too,
                // but checking them early saves time.
                if(!seatCompatible(e, veh[v])) continue;
                if(!premiumCompatible(e, veh[v])) continue;

                Route tmp = sol[v];
                tmp.seq.push_back(e.id);

                if (!routeFeasible(tmp, veh[v], emp)) continue;

                double c = routeCost(tmp, veh[v], emp);
                if(c < bestCost){
                    bestCost = c;
                    bestVeh = v;
                }
            }

            if(bestVeh != -1) {
                sol[bestVeh].seq.push_back(e.id);
            }
        }
    }
}

void randomRepair(std::vector<Route>& sol,
                  const std::vector<Employee>& emp,
                  const std::vector<Vehicle>& veh){

    static std::mt19937 rng(123);

    for(const auto& e:emp){
        bool used=false;
        for(auto& r:sol){
            if(std::find(r.seq.begin(),r.seq.end(),e.id)!=r.seq.end()){
                used=true; break;
            }
        }
        if(!used){
            if (sol.empty()) break;
            
            // Find all feasible vehicles
            std::vector<int> feasibleVehs;
            for(size_t v=0; v<sol.size(); ++v) {
                if(!seatCompatible(e, veh[v])) continue;
                if(!premiumCompatible(e, veh[v])) continue;
                
                // Check Route Feasibility
                Route tmp = sol[v];
                tmp.seq.push_back(e.id);
                if(routeFeasible(tmp, veh[v], emp)) {
                    feasibleVehs.push_back(v);
                }
            }
            
            if (!feasibleVehs.empty()) {
                int idx = std::uniform_int_distribution<>(0, feasibleVehs.size() - 1)(rng);
                int v = feasibleVehs[idx];
                sol[v].seq.push_back(e.id);
            } else {
                // If no feasible vehicle found, we skip (leave unassigned)
                // or force assign to random (and rely on cost penalty)?
                // Since we have hard constraints now, force assign might break things?
                // But ALNS prefers complete solutions.
                // Let's try force assign to a random PREMIUM compatible one if possible?
                // Or just random one?
                // Let's stick to FEASIBLE ONLY. If infeasible, logic implies we can't service?
                // But usually we can.
                // Fallback: Pick any seat/premium compatible?
                std::vector<int> compatVehs;
                 for(size_t v=0; v<sol.size(); ++v) {
                    if(seatCompatible(e, veh[v]) && premiumCompatible(e, veh[v])) {
                        compatVehs.push_back(v);
                    }
                }
                
                if (!compatVehs.empty()) {
                     int idx = std::uniform_int_distribution<>(0, compatVehs.size() - 1)(rng);
                     int v = compatVehs[idx];
                     sol[v].seq.push_back(e.id);
                } else {
                     // Last Resort: Any random vehicle (Cost function will penalize)
                      int v = std::uniform_int_distribution<>(0, sol.size() - 1)(rng);
                      sol[v].seq.push_back(e.id);
                }
            }
        }
    }
}

void regretRepair(std::vector<Route>& sol,
                  const std::vector<Employee>& emp,
                  const std::vector<Vehicle>& veh,
                  int k){

    std::vector<bool> assigned(emp.size(), false);
    for(const auto& r:sol)
        for(int id:r.seq) assigned[id]=true;

    while(true){
        int bestEmp = -1;
        double bestRegret = -1;
        int bestVeh = -1;
        bool anyUnassigned = false;

        for(const auto& e:emp){
            if(assigned[e.id]) continue;
            anyUnassigned = true;

            std::vector<double> costs;

            for(size_t v=0; v<sol.size(); v++){
                if(!seatCompatible(e, veh[v])) continue;
                if(!premiumCompatible(e, veh[v])) continue;

                Route tmp = sol[v];
                tmp.seq.push_back(e.id);

                if (!routeFeasible(tmp, veh[v], emp)) continue;

                double c = routeCost(tmp, veh[v], emp);
                costs.push_back(c);
            }

            if(costs.empty()) continue;

            std::sort(costs.begin(), costs.end());

            double regret = 0;
            if (costs.size() >= k) {
                regret = costs[k-1] - costs[0];
            } else if (costs.size() > 1) {
                regret = costs.back() - costs[0];
            } else {
                regret = costs[0];
            }

            if(regret > bestRegret){
                bestRegret = regret;
                bestEmp = e.id;
            }
        }

        if(bestEmp == -1) {
             if (anyUnassigned) {
                 // Fallback: find first unassigned and assign greedily
                 for(const auto& e:emp) {
                     if(!assigned[e.id]) {
                         double bestCost = std::numeric_limits<double>::max();
                         int bVeh = -1;
                         for(size_t v=0; v<sol.size(); v++){
                            if(!seatCompatible(e, veh[v])) continue;
                            if(!premiumCompatible(e, veh[v])) continue;
                            Route tmp = sol[v];
                            tmp.seq.push_back(e.id);

                            if (!routeFeasible(tmp, veh[v], emp)) continue;

                            double c = routeCost(tmp, veh[v], emp);
                            if(c < bestCost){
                                bestCost = c;
                                bVeh = v;
                            }
                         }
                         if (bVeh != -1) {
                             sol[bVeh].seq.push_back(e.id);
                             assigned[e.id] = true;
                             anyUnassigned = true;
                             // Instead of goto, we can just continue the outer loop
                             // But we need to signal that we found something.
                             // Since we are inside the 'if(bestEmp == -1)' block,
                             // we can just set bestEmp and bestVeh here?
                             // No, because the logic below expects bestEmp to be set by regret logic.
                             // But we can just do the assignment here and continue the while loop.
                             continue;
                         }
                     }
                 }
             }
             break;
        }

        double bestCost = std::numeric_limits<double>::max();
        for(size_t v=0; v<sol.size(); v++){
            if(!seatCompatible(emp[bestEmp], veh[v])) continue;
            if(!premiumCompatible(emp[bestEmp], veh[v])) continue;

            Route tmp = sol[v];
            tmp.seq.push_back(bestEmp);

            if (!routeFeasible(tmp, veh[v], emp)) continue;

            double c = routeCost(tmp, veh[v], emp);
            if(c < bestCost){
                bestCost = c;
                bestVeh = v;
            }
        }

        if (bestVeh != -1) {
            sol[bestVeh].seq.push_back(bestEmp);
            assigned[bestEmp] = true;
        } else {
            assigned[bestEmp] = true;
        }
    }
}
