#pragma once
#include "Route.h"
#include "Employee.h"
#include "Vehicle.h"
#include <vector>

void greedyRepair(std::vector<Route>&,
                  const std::vector<Employee>&,
                  const std::vector<Vehicle>&);

void randomRepair(std::vector<Route>&,
                  const std::vector<Employee>&,
                  const std::vector<Vehicle>&);

void regretRepair(std::vector<Route>&,
                  const std::vector<Employee>&,
                  const std::vector<Vehicle>&,
                  int k = 2);
