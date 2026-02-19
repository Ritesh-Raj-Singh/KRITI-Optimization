#include "CostFunction.h"
#include "Distance.h"

#include <algorithm>
#include <vector>
#include <cmath>

static double priorityPenalty(int p){
    return 0.0 * (6-p) * (6-p); 
}

CostComponents getRouteCostComponents(const Route& r, const Vehicle& v, const std::vector<Employee>& emp, const Metadata& meta){
    CostComponents cc = {0.0, 0.0, 0.0};
    if (r.seq.empty()) return cc;

    std::vector<double> pickupTime(emp.size(), -1); //1

    double t = v.startTime;
    double t1=0.0;
    
    double cx = v.x, cy = v.y;
    double totalDist = 0.0;
    double penaltyCost = 0.0;
    
    std::vector<int> batch; 
    
    auto processBatch = [&]( double& currT, double& currX, double& currY) {
        if (batch.empty()) return;
        

        const auto& last = emp[batch.back()]; 
        double dOff = distKm(currX, currY, last.destX, last.destY);
        double travelT = (dOff / v.speed) * 60.0;

        totalDist += dOff;
        
        double arrival = currT + travelT;
        for (int id : batch) {
            t1 += (arrival - pickupTime[id]);
        }
        currT = arrival;
        for (int id : batch) {
            const auto& e = emp[id];
            
            double due = e.due +getMaxLateness(e.priority,meta);
            if (arrival > due) {
                double lateMins = arrival - due;
                // Linear penalty: 100000 * lateMins
                penaltyCost += 100000.0 * lateMins;

            } else {

            }
        }
        
        currX = last.destX;
        currY = last.destY;
        batch.clear();
    };

    for (int eId : r.seq) {
        const auto& e = emp[eId];

        // Vehicle Mismatch Penalty
        if (e.vehiclePref == "premium" && !v.premium) {
            penaltyCost += 100000.0;
        } 
        else if (e.vehiclePref == "normal" && v.premium) {
             // No penalty for normal in premium
        }
        // "any" gets no penalty
        
        bool fits = true;
        if (batch.size() + 1 > v.seatCap) fits = false;
        else {
            if (e.sharePref < (int)batch.size() + 1) fits = false;
            for (int bid : batch) if (emp[bid].sharePref < (int)batch.size() + 1) fits = false;
        }
        
        if (!fits) {
            processBatch(t, cx, cy);
            double d = distKm(cx, cy, e.x, e.y);
            totalDist += d;
            t += ((d / v.speed) * 60.0);  

        } else {
        }
        if (!batch.empty() && fits) { 
             double d = distKm(cx, cy, e.x, e.y);
             totalDist += d;
             t += (d / v.speed) * 60.0;

        } else if (batch.empty()) {
             if (fits) {
                 double d = distKm(cx, cy, e.x, e.y);
                 totalDist += d;
                 t += (d / v.speed) * 60.0;

             }
        }

        if(t<e.ready){

        }
        double startService = std::max(t, e.ready);
        t = startService;
        cx = e.x; cy = e.y;
        pickupTime[eId] = t;
        batch.push_back(eId); //2
    }

    if (!batch.empty()) {
        processBatch(t, cx, cy);
    }

    if (t > v.endTime) {
         penaltyCost += 10000.0 * (t - v.endTime); 
    }


    
    double travelMoneyCost = totalDist * v.costPerKm;
    double duration = t1;
    
    double operationalCost = (travelMoneyCost * meta.objectiveCostWeight) + (duration * meta.objectiveTimeWeight);
    
    cc.operationalCost = operationalCost;
    cc.penaltyCost = penaltyCost;
    cc.totalCost = operationalCost + penaltyCost;
    
    return cc;
}

double routeCost(const Route& r, const Vehicle& v, const std::vector<Employee>& emp, const Metadata& meta){
    if (r.seq.empty()) return 0.0;
    if (!r.isDirty) return r.cachedCost;

    CostComponents cc = getRouteCostComponents(r, v, emp, meta);
    
    r.cachedCost = cc.totalCost;
    r.isDirty = false;
    return r.cachedCost;
}

