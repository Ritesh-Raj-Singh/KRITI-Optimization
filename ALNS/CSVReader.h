#pragma once
#include <vector>
#include <string>
#include "Employee.h"
#include "Vehicle.h"

std::vector<Employee> readEmployees(const std::string& path);
std::vector<Vehicle> readVehicles(const std::string& path);
