#pragma once
#include "Route.h"
#include "Vehicle.h"
#include "Employee.h"
#include <vector>

#include "CSVReader.h" // For Metadata

struct CostComponents {
    double operationalCost;
    double penaltyCost;
    double totalCost;
};

CostComponents getRouteCostComponents(const Route&, const Vehicle&, const std::vector<Employee>&, const Metadata&);
double routeCost(const Route&, const Vehicle&, const std::vector<Employee>&, const Metadata&);

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
