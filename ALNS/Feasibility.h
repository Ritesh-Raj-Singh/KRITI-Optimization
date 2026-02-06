#pragma once
#include "Route.h"
#include "Vehicle.h"
#include "Employee.h"
#include "Compatibility.h"
#include "Distance.h"
#include <vector>
#include <algorithm>
#include <cmath>

// We need a helper to simulate the route and return valid/invalid.
// Since we also need cost, maybe we should unify simulation?
// For now, let's keep them separate but share logic if possible.
// Or just inline the logic.

// Logic:
// The Route.seq is the order of PICKUPS.
// The vehicle starts at V.start.
// It acts as a "Shuttle" to the Office.
// It can handle multiple "Trips" if necessary, but that complicates sequence decoding.
// Simplified Model:
// - Vehicle picks up everyone in the sequence order.
// - It goes to the Office ONLY when:
//    a) It needs to drop people to free up capacity (for next pickups).
//    b) It needs to meet a deadline (Employee Due Time).
//    c) It has finished all pickups.
//
// But "Drop to free capacity" requires the current passengers to be dropped.
// If we drop everyone, we are empty.
// So the structure is: [Pickup A, Pickup B, ...] -> Drop -> [Pickup C, ...] -> Drop.
// This means the sequence represents a list of clusters.
//
// Challenge: ALNS just permutes the list. It doesn't insert "Drop".
// We must infer the drops.
// Greedy Strategy:
// Keep picking up until:
// 1. Capacity would be exceeded.
// 2. SharePref would be violated (e.g. picking up C would make 3 people, but A only tolerates 2).
// 3. Time Constraints (Pickup C would make us late for A's drop off?).
//
// If we stop picking up, we MUST go to Office and Drop ALL current passengers.
// Then proceed to next pickup.
//
// Refined Logic (Batched Trip):
// Loop through seq:
//   Try to add next employee 'e' to current Batch.
//   Check:
//     - Physical Capacity: (current_batch_size + 1) <= v.seatCap
//     - Share Compatibility:
//         - For all p in Match: p.sharePref >= (current_batch_size + 1)
//         - e.sharePref >= (current_batch_size + 1)
//     - Time Feasibility:
//         - Estimate Arrival at e (current_time + travel).
//         - Check e.ready restriction (wait if needed).
//         - Check if we can still reach Office by MIN(p.due) for all p in batch, including e.
//         - Check if we can reach Office and effectively service them?
//   If Valid:
//     Add e to Batch. Update location, time.
//   If Invalid:
//     "Close" the batch.
//     Drive to Office. Drop everyone.
//     Time = MAX(Time_Arrival_Office, Previous_Drop_Time + Service?).
//     Empty Batch.
//     Drive from Office to 'e'.
//     Start new Batch with 'e'.
//
//   Check V.endTime at the very end.

inline bool routeFeasible(const Route& r,
                          const Vehicle& v,
                          const std::vector<Employee>& emp){

    if (r.seq.empty()) return true;

    double t = v.startTime;
    double cx = v.x, cy = v.y;
    
    // Batch state
    std::vector<int> batch; 
    
    // Helper to check batch validity
    auto checkBatch = [&](const std::vector<int>& b, int nextId) -> bool {
        // Size check
        if (b.size() + 1 > v.seatCap) return false;
        
        const auto& nextE = emp[nextId];
        int newSize = b.size() + 1;
        
        // Share Pref Check for new guy
        if (nextE.sharePref < newSize) return false;
        
        // Share Pref Check for existing guys
        for (int pid : b) {
            if (emp[pid].sharePref < newSize) return false;
        }
        
        return true;
    };

    for (size_t i = 0; i < r.seq.size(); ++i) {
        int eId = r.seq[i];
        const auto& e = emp[eId];

        // 0. Compatibility Check (Premium/Capacity Basic)
        // Ensure this basic constraint is not violated by the sequence
        if (!premiumCompatible(e, v)) return false;
        bool fits = checkBatch(batch, eId);
        
        // 2. Check time feasibility (lookahead)
        // If we add e, we travel from cx,cy to e.x,e.y
        double travel = dist(cx, cy, e.x, e.y) / v.speed * 60.0; // Minutes? Speed is km/h? 
        // Problem: CSV avg speed is likely km/h. Dist is Euclidean degrees?
        // CSV coordinates look like lat/long (12.x, 77.x).
        // Euclidean on Lat/Long is bad but likely intended for this toy problem.
        // Or we need Haversine. 
        // `dist` function in Distance.h is `sqrt(dx*dx + dy*dy)`.
        // Degrees to km conversion: approx 111km per degree.
        // Let's assume the speed and dist match units. 
        // If speed is 30 (km/h) and dist is 0.1 (degrees), 0.1 deg ~ 11km. 
        // 11km / 30km/h = 0.36 hrs = 22 mins.
        // We need a conversion factor.
        // Let's assume standard approx: 1 deg ~ 111 km.
        // But `dist` returns raw sqrt.
        // Let's modify `Distance.h` or handle it here.
        // Let's assume input speed is km/h.
        // Let's apply a factor 111.0 to dist?
        // Let's check `CostFunction` in old code. `travel = dist()/v.speed`.
        // It didn't have conversion.
        // Maybe x,y are Cartesian km?
        // 12.9, 77.4 look like Bangalore coordinates.
        // So they are Lat/Long.
        // We MUST convert to km to make sense of Speed (km/h) and Time (mins).
        // Factor ~ 111 km per degree (lat). Longitude varies but ~100?
        // Let's use 100.0 as approximate constant conversion to km.
        
        double dKm = distKm(cx, cy, e.x, e.y); 
        double travelMin = (dKm / v.speed) * 60.0;
        
        double arrival = t + travelMin;
        double startService = std::max(arrival, e.ready);
        double depart = startService; // 1 min pickup service  

        // NOW: Can we reach Office by deadline?
        // Distance to dest
        double dDestKm = distKm(e.x, e.y, e.destX, e.destY);
        double timeToDest = (dDestKm / v.speed) * 60.0;
        double arrivalAtDest = depart + timeToDest;
        
        // Deadline check for e
        if (arrivalAtDest > e.due + getMaxLateness(e.priority)) fits = false; // Strict feasibility check with buffer
        // Feasibility.h said "CostFunction uses soft time windows".
        // BUT "Feasibility checks" usually enforce Hard constraints or basic viability.
        // "We can give a penalty ... if time window is exceeded upto some extra time".
        // This implies Hard Limit = Due + Extra?
        // Or Soft everywhere?
        // "All employees must be assigned". 
        // If Feasibility checks strict windows, we might fail to assign.
        // Let's allow minor lateness in Feasibility, but Cost handles it.
        // But strict "Vehicle End Time" applies.
        
        // Also check if making this detour makes previous batch members late?
        // If we pick up E, we delay everyone in batch by (TravelToE + Service).
        // Simplified: We verify the *entire path* of the batch.
        // Pickup path: B1 -> B2 -> ... -> Bk -> Office.
        // If we add E: B1 -> ... -> Bk -> E -> Office.
        // Recalculate arrival at office.
        // Check if ANYONE in batch is effectively late?
        // Actually, everyone is dropped at Office at the SAME time.
        // So we just check arrivalAtDest vs MIN(Batch.due).
        
        // Re-eval batch if we add E:
        if (fits) {
             // Simulate path for batch + E
             // Current T is at 'cx' (prev loc).
             // Path: cx -> e -> Office.
             // Time at Office = depart + timeToDest.
             // Check all batch dues.
             for (int bid : batch) {
                 if (arrivalAtDest > emp[bid].due) {
                     // Lateness check.
                     // If existing member becomes late beyond allowed buffer, it's infeasible.
                     if (arrivalAtDest > emp[bid].due + getMaxLateness(emp[bid].priority)) fits = false;
                     // Let's be semi-strict for Feasibility:
                     // Max Delay?
                     // Let's assume "Feasible" means "Not ridiculously late".
                     // Or just check vehicle constraints.
                 }
             }
        }

        if (fits) {
            // Add to batch
            batch.push_back(eId);
            t = depart;
            cx = e.x; 
            cy = e.y;
        } else {
            // Must drop current batch first
            if (batch.empty()) return false; // Should not happen if fits check failed for single person? 
            // If empty batch fails, it means e itself is infeasible (e.g. late).
            // Return false.
            if (batch.empty()) return false;

            // Deliver Batch
            // Dist from current (last pickup) to Office
            const auto& last = emp[batch.back()];
            double dOff = distKm(cx, cy, last.destX, last.destY);
            double tOff = (dOff / v.speed) * 60.0;
            t += tOff;
           // t += 1.0; // Dropoff service?  -----------------
            
            // Check vehicle end
            if (t > v.endTime) return false;
            
            // Clear batch
            batch.clear();
            
            // Drive from Office to e
            // Current loc is Office (last.destX)
            double dToE = distKm(last.destX, last.destY, e.x, e.y);
            t += (dToE / v.speed) * 60.0;
            
            // Check E feasibility again (as new batch)
            // ...
            // Simplification: Recursively/Iteratively continue.
            // Reset cx, cy to Office before loop step?
            // Actually, we just transitioned T and Loc.
            // Now attempt to add E again in next iter?
            // No, we are in the loop for E.
            // We just performed the drop. Now we process E.
            
            cx = last.destX; cy = last.destY;
            
            // Re-run checks for E as first of new batch
            // Physical capacity: 1 <= SeatCap (True)
            // SharePref: e.sharePref >= 1 (True)
            // Time:
            dKm = distKm(cx, cy, e.x, e.y);
            travelMin = (dKm / v.speed) * 60.0;
            arrival = t + travelMin;
            startService = std::max(arrival, e.ready);
            depart = startService + 1.0;
            
            dDestKm = distKm(e.x, e.y, e.destX, e.destY);
            timeToDest = (dDestKm / v.speed) * 60.0;
            arrivalAtDest = depart + timeToDest;
            
            // If Late?
            // if (arrivalAtDest > e.due) ...
            
            if (arrivalAtDest > v.endTime) return false; // Definitive fail
            
            batch.push_back(eId);
            t = depart;
            cx = e.x; cy = e.y;
        }
    }
    
    // Final drop
    if (!batch.empty()) {
        const auto& last = emp[batch.back()];
        double dOff = distKm(cx, cy, last.destX, last.destY);
        t += (dOff / v.speed) * 60.0;
        if (t > v.endTime) return false;
    }
    
    return true;
}
