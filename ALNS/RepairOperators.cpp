#include "RepairOperators.h"
#include <random>
#include <algorithm>
#include "CostFunction.h"
#include <limits>

void greedyRepair(std::vector<Route> &sol,
                  const std::vector<Employee> &emp,
                  const std::vector<Vehicle> &veh,
                  const Metadata &meta)
{
    std::vector<int> order;
    for (const auto &e : emp)
        order.push_back(e.id);

    std::sort(order.begin(), order.end(), [&](int a, int b)
              {
                  // Premium/Normal employees have priority over "any"
                  bool aPriority = (emp[a].vehiclePref != "any");
                  bool bPriority = (emp[b].vehiclePref != "any");
                  return aPriority > bPriority;
              });

    std::vector<bool> assigned(emp.size(), false);
    for (const auto &r : sol)
        for (int id : r.seq)
            assigned[id] = true;

    for (int eId : order)
    {   if(assigned[eId]) continue;
        const auto &e = emp[eId];
        if (1)
        {
            double bestCost = std::numeric_limits<double>::max();
            int bestVeh = -1;
            for (size_t v = 0; v < sol.size(); v++)
            {   
                if(veh[v].speed<=0.0 || veh[v].seatCap<1) continue;

                Route tmp = sol[v];
                tmp.seq.push_back(e.id);
                tmp.isDirty = true;

                double c = routeCost(tmp, veh[v], emp, meta);
                if (c < bestCost)
                {
                    bestCost = c;
                    bestVeh = v;
                }
            }

            if (bestVeh != -1)
            {
                sol[bestVeh].seq.push_back(e.id);
                assigned[e.id]=true;
                sol[bestVeh].isDirty = true;
            }
        }
    }
}




void randomRepair(std::vector<Route> &sol,
                  const std::vector<Employee> &emp,
                  const std::vector<Vehicle> &veh,
                  const Metadata &meta)
{

    static std::mt19937 rng(42);
    std::vector<bool> assigned(emp.size(), false);
    for (const auto &r : sol)
        for (int id : r.seq)
            assigned[id] = true;
    for (const auto &e : emp)
    {
        
        if (assigned[e.id])
            continue;
        if (1)
        {
            if (sol.empty())
                break;

            std::vector<int> feasibleVehs;
            std::vector<int> PosVehs;
            for (size_t v = 0; v < sol.size(); ++v)
            {
                if(veh[v].speed<=0.0 || veh[v].seatCap<1) continue;
                Route tmp = sol[v];
                tmp.seq.push_back(e.id);
                tmp.isDirty = true;
                PosVehs.push_back(v);
               
                    feasibleVehs.push_back(v);
            
            }

            if (!feasibleVehs.empty())
            {
                int idx = std::uniform_int_distribution<>(0, feasibleVehs.size() - 1)(rng);
                int v = feasibleVehs[idx];
                sol[v].seq.push_back(e.id);
                assigned[e.id] = true;
                sol[v].isDirty = true;
            }
            else
            {
                // int v = std::uniform_int_distribution<>(0, sol.size() - 1)(rng);
                 int idx = std::uniform_int_distribution<>(0, PosVehs.size() - 1)(rng);
                int v = PosVehs[idx];
                sol[v].seq.push_back(e.id);
                assigned[e.id] = true;
                sol[v].isDirty = true;
            }
        }
    }
}

void regretRepair(std::vector<Route> &sol,
                  const std::vector<Employee> &emp,
                  const std::vector<Vehicle> &veh,
                  const Metadata &meta,
                  int k)
{

    std::vector<bool> assigned(emp.size(), false);
    for (const auto &r : sol)
        for (int id : r.seq)
            assigned[id] = true;

    while (true)
    {
        int bestEmp = -1;
        double bestRegret = -1;
        int bestVeh = -1;
        bool anyUnassigned = false;

        for (const auto &e : emp)
        {
            if (assigned[e.id])
                continue;
            anyUnassigned = true;

            std::vector<double> costs;

            for (size_t v = 0; v < sol.size(); v++)
            { 
               if(veh[v].speed<=0.0 || veh[v].seatCap<1) continue;
                Route tmp = sol[v];
                tmp.seq.push_back(e.id);
                tmp.isDirty = true;


                double c = routeCost(tmp, veh[v], emp, meta);
                costs.push_back(c);
            }

            if (costs.empty())
                continue;

            std::sort(costs.begin(), costs.end());

            double regret = 0;
            if (costs.size() >= k)
            {
                regret = costs[k - 1] - costs[0];
            }
            else if (costs.size() > 1)
            {
                regret = costs.back() - costs[0];
            }
            else
            {
                regret = costs[0];
            }

            if (regret > bestRegret)
            {
                bestRegret = regret;
                bestEmp = e.id;
            }
        }

        if (bestEmp == -1)
        {
            if (anyUnassigned)
            {
                for (const auto &e : emp)
                {
                    if (!assigned[e.id])
                    {
                        double bestCost = std::numeric_limits<double>::max();
                        int bVeh = -1;
                        for (size_t v = 0; v < sol.size(); v++)
                        {
                           if(veh[v].speed<=0.0 || veh[v].seatCap<1) continue;
                            Route tmp = sol[v];
                            tmp.seq.push_back(e.id);
                            tmp.isDirty = true;

                         

                            double c = routeCost(tmp, veh[v], emp, meta);
                            if (c < bestCost)
                            {
                                bestCost = c;
                                bVeh = v;
                            }
                        }
                        if (bVeh != -1)
                        {
                            sol[bVeh].seq.push_back(e.id);
                            sol[bVeh].isDirty = true;
                            assigned[e.id] = true;
                            anyUnassigned = true;
                            continue;
                        }
                    }
                }
            }
            break;
        }

        double bestCost = std::numeric_limits<double>::max();
        for (size_t v = 0; v < sol.size(); v++)
        {
            if(veh[v].speed<=0.0 || veh[v].seatCap<1) continue;
            Route tmp = sol[v];
            tmp.seq.push_back(bestEmp);
            tmp.isDirty = true;

            double c = routeCost(tmp, veh[v], emp, meta);
            if (c < bestCost)
            {
                bestCost = c;
                bestVeh = v;
            }
        }

        if (bestVeh != -1)
        {
            sol[bestVeh].seq.push_back(bestEmp);
            sol[bestVeh].isDirty = true;
            assigned[bestEmp] = true;
        }
        else
        {
            assigned[bestEmp] = true;
        }
 
    }
}
