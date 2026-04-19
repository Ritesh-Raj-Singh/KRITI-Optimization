#pragma once
#include <random>
#include <vector>

#include "Employee.h"
#include "Route.h"
#include "Vehicle.h"

void randomDestroy(std::vector<Route>&, int q);
void LongestRouteTailRemoval(std::vector<Route>&, int q);
void vehicleDestroy(std::vector<Route>&);
#include "CSVReader.h"
void WorstRouteTailRemoval(std::vector<Route>&, const std::vector<Employee>&, const std::vector<Vehicle>&,
                           const Metadata&);
