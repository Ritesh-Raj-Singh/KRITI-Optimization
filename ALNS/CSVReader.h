#pragma once
#include <vector>
#include <string>
#include "Employee.h"
#include "Vehicle.h"

struct Metadata {
    // Weights
    double objectiveCostWeight = 0.7;
    double objectiveTimeWeight = 0.3;

    // Delays (in minutes)
    int priority_1_max_delay_min = 5;
    int priority_2_max_delay_min = 10;
    int priority_3_max_delay_min = 15;
    int priority_4_max_delay_min = 20;
    int priority_5_max_delay_min = 30;
};

std::vector<Employee> readEmployees(const std::string& path);
std::vector<Vehicle> readVehicles(const std::string& path);
Metadata readMetadata(const std::string& path);
void readDist( const std::string& filename,int max_size);