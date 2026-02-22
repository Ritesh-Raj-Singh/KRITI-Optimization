#pragma once
#include "Route.h"
#include "Employee.h"
#include "Vehicle.h"
#include <vector>

#include "CSVReader.h"

void greedyRepair(std::vector<Route>&,
                  const std::vector<Employee>&,
                  const std::vector<Vehicle>&,
                  const Metadata&);

void randomRepair(std::vector<Route>&,
                  const std::vector<Employee>&,
                  const std::vector<Vehicle>&,
                  const Metadata&);

void regretRepair(std::vector<Route>&,
                  const std::vector<Employee>&,
                  const std::vector<Vehicle>&,
                  const Metadata&,
                  int k = 2);
