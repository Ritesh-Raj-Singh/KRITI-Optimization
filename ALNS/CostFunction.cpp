#include "CostFunction.h"
#include "Distance.h"
#include "Compatibility.h" 
#include <algorithm>
#include <vector>
#include <cmath>

static double priorityPenalty(int p){
    return 0.0 * (6-p) * (6-p); 
}

double routeCost(const Route& r, const Vehicle& v, const std::vector<Employee>& emp, const Metadata& meta){
    if (r.seq.empty()) return 0.0;

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
        t1+= travelT*batch.size();
        totalDist += dOff;
        
        double arrival = currT + travelT;
        currT = arrival;
        for (int id : batch) {
            const auto& e = emp[id];
            
            double due = e.due +getMaxLateness(e.priority,meta);
            if (arrival > due) {
                double lateMins = arrival - due;
                // Exponential penalty: 2^(20 + 15/180 * lateMins)
                // Scales from 2^20 at 0 min to 2^35 at 180 min
                double exponent = 40.0 + (40.0 / 180.0) * lateMins;
                penaltyCost += std::pow(2.0, exponent);
            }
        }
        
        currX = last.destX;
        currY = last.destY;
        batch.clear();
    };

    for (int eId : r.seq) {
        const auto& e = emp[eId];

        // Vehicle Mismatch Penalty (2^20)
        if (e.vehiclePref == "premium" && !v.premium) {
            penaltyCost += std::pow(2.0, 20.0);
        } 
        else if (e.vehiclePref == "normal" && v.premium) {
            penaltyCost += std::pow(2.0, 20.0);
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
            t1 += batch.size()*((d / v.speed) * 60.0);  
        } else {
        }
        if (!batch.empty() && fits) { 
             double d = distKm(cx, cy, e.x, e.y);
             totalDist += d;
             t += (d / v.speed) * 60.0;
             t1 += batch.size()*((d / v.speed) * 60.0);
        } else if (batch.empty()) {
             if (fits) {
                 double d = distKm(cx, cy, e.x, e.y);
                 totalDist += d;
                 t += (d / v.speed) * 60.0;
                  t1 += batch.size()*((d / v.speed) * 60.0);
             }
        }

        if(t<e.ready){
            t1+=(e.ready-t)*batch.size();
        }
        double startService = std::max(t, e.ready);
        t = startService;
        cx = e.x; cy = e.y;
        batch.push_back(eId);
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
    
    return operationalCost + penaltyCost;
}

