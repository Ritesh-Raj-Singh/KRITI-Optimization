#pragma once
#include <vector>

#include "CSVReader.h"
#include "Employee.h"
#include "Route.h"
#include "Vehicle.h"
#include "globals.h"

std::vector<Route> solveALNS(const std::vector<Employee> &, const std::vector<Vehicle> &, const Metadata &);
