#pragma once
#include "Employee.h"
#include "Vehicle.h"
#include "Route.h"
#include <vector>
#include "CSVReader.h" 

std::vector<Route> solveALNS(
    const std::vector<Employee>&,
    const std::vector<Vehicle>&,
    const Metadata& 
);
