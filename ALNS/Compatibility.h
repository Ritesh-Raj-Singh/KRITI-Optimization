#pragma once
#include "Employee.h"
#include "Vehicle.h"
#include "CSVReader.h"
#include <algorithm>

inline bool seatCompatible(const Employee& e, const Vehicle& v){
    return v.seatCap >= 1;
}

inline bool premiumCompatible(const Employee& e, const Vehicle& v){
    // Relaxed: All combinations are feasible. Penalty is applied in CostFunction.
    return true; 
}

// static const double PRIORITY_1_MAX_LATE = 5.0;
// static const double PRIORITY_2_MAX_LATE = 10.0;
// static const double PRIORITY_3_MAX_LATE = 15.0;
// static const double PRIORITY_4_MAX_LATE = 20.0;
// static const double PRIORITY_5_MAX_LATE = 30.0;

inline double getMaxLateness(int priority, Metadata meta) {
    switch(priority) {
        case 1: return meta.priority_1_max_delay_min;
        case 2: return meta.priority_2_max_delay_min;
        case 3: return meta.priority_3_max_delay_min;
        case 4: return meta.priority_4_max_delay_min;
        case 5: return meta.priority_5_max_delay_min;
        default: return meta.priority_5_max_delay_min; 
    }
}

