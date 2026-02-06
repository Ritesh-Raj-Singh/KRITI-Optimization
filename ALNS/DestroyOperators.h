#pragma once
#include "Route.h"
#include <vector>
#include <random>
#include "Employee.h"
#include "Vehicle.h"

void randomDestroy(std::vector<Route>&, int q);
void worstCostDestroy(std::vector<Route>&, int q);
void vehicleDestroy(std::vector<Route>&);
void timeWindowDestroy(std::vector<Route>&,
                       const std::vector<Employee>&,
                       const std::vector<Vehicle>&);
