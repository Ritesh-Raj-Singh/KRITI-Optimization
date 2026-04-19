#pragma once
#include <vector>

#include "CSVReader.h"
#include "Employee.h"
#include "Route.h"
#include "Vehicle.h"

void greedyRepair(std::vector<Route> &, const std::vector<Employee> &, const std::vector<Vehicle> &, const Metadata &);

void randomRepair(std::vector<Route> &, const std::vector<Employee> &, const std::vector<Vehicle> &, const Metadata &);

void regretRepair(std::vector<Route> &, const std::vector<Employee> &, const std::vector<Vehicle> &, const Metadata &,
                  int k = 2);
