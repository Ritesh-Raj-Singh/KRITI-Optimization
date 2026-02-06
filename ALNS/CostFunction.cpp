#include "CostFunction.h"
#include "Distance.h"
#include "Compatibility.h" // Added for getMaxLateness
#include <algorithm>
#include <vector>
#include <cmath>

static double priorityPenalty(int p){
    // p is priority level. 1 to 5?
    // "Give a penalty based on priority level if time window is exceeded"
    // Higher priority = Higher penalty?
    // Or Priority 1 is highest?
    // CSV: "User priority / level".
    // Assumption: Higher P = Higher importance? Or 1 is top?
    // Usually 1 is top.
    // Let's assume P is a multiplier.
    return 0.0 * (6-p) * (6-p); 
}

double routeCost(const Route& r, const Vehicle& v, const std::vector<Employee>& emp){
    if (r.seq.empty()) return 0.0;

    double t = v.startTime;
    double cx = v.x, cy = v.y;
    double totalDist = 0.0;
    double penaltyCost = 0.0;
    
    std::vector<int> batch; 
    
    auto processBatch = [&](double& currT, double& currX, double& currY) {
        if (batch.empty()) return;
        
        // Drive current batch to Office
        const auto& last = emp[batch.back()]; 
        double dOff = distKm(currX, currY, last.destX, last.destY);
        double travelT = (dOff / v.speed) * 60.0;
        
        // Track Distance
        totalDist += dOff;
        
        double arrival = currT + travelT;
        currT = arrival; // Drop time (No service time)
        
        // Late Penalties
        for (int id : batch) {
            const auto& e = emp[id];
            double due = e.due;
            if (arrival > due) {
                double lateMins = arrival - due;
                if (lateMins > getMaxLateness(e.priority)) {
                    penaltyCost += 1e20; // Hard limit penalty
                } else {
                    penaltyCost += priorityPenalty(e.priority) * lateMins;
                }
            }
        }
        
        currX = last.destX;
        currY = last.destY;
        batch.clear();
    };

    for (int eId : r.seq) {
        const auto& e = emp[eId];

        // Premium Constraint Penalty
        if (e.wantsPremium && !v.premium) {
             penaltyCost += 1e9;
        }
        
        bool fits = true;
        if (batch.size() + 1 > v.seatCap) fits = false;
        else {
            if (e.sharePref < (int)batch.size() + 1) fits = false;
            for (int bid : batch) if (emp[bid].sharePref < (int)batch.size() + 1) fits = false;
        }
        
        if (!fits) {
            // deliver batch
            processBatch(t, cx, cy);
            
            // Drive to E from Office
            double d = distKm(cx, cy, e.x, e.y);
            totalDist += d;
            t += (d / v.speed) * 60.0;
        } else {
            // Check time split logic? 
            // Only capacity split is implemented here for now as discussed before.
        }
        
        // Add E to batch - Travel from cx to e
        if (!batch.empty() && fits) { 
             double d = distKm(cx, cy, e.x, e.y);
             totalDist += d;
             t += (d / v.speed) * 60.0;
        } else if (batch.empty()) {
             // Already moved to E if !fits case above? 
             // If !fits, we moved Office->E.
             // If fits, we move Prev->E.
             // Wait, the logic block above handles movement if !fits.
             // If fits, we still need to move.
             if (fits) {
                 double d = distKm(cx, cy, e.x, e.y);
                 totalDist += d;
                 t += (d / v.speed) * 60.0;
             }
             // If !fits, we already added distance Office->E.
        }

        double startService = std::max(t, e.ready);
        t = startService;
        cx = e.x; cy = e.y;
        batch.push_back(eId);
    }
    
    // Final batch
    if (!batch.empty()) {
        processBatch(t, cx, cy);
    }
    
    // Vehicle Overtime Penalty
    if (t > v.endTime) {
         penaltyCost += 10000.0 * (t - v.endTime); // Huge penalty
    }

    // Calculate Final Objective Cost
    // User Update: 0.7 * (Distance * CostPerKm) + 0.3 * Time
    
    // We were tracking totalDist. But vehicles have specific costPerKm.
    // We need to calculate the Money cost.
    // Wait, totalDist was accumulated in the loop. 
    // Is CostFunction passed a single vehicle? Yes 'v'.
    // So distinct cost per km applies to the whole route.
    
    double travelMoneyCost = totalDist * v.costPerKm;
    double duration = t - v.startTime;
    
    double operationalCost = (travelMoneyCost * 0.7) + (duration * 0.3);
    
    return operationalCost + penaltyCost;
}
