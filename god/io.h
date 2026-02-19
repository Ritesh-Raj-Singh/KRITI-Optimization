#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <cctype>

#include "model.h"
#include "utils.h"

int id_from_string(const std::string &vehicle_id_str);

int read_vehicle_data(std::string file_name, DARPInstance &instance);

int read_employee_data(std::string file_name, DARPInstance &instance);

void read_metadata(std::string file_name);

extern std::map<int, int> PRIORITY_DELAYS;
extern double WEIGHT_COST;
extern double WEIGHT_TIME;

void loadMatrix(const std::string &filename, DARPInstance &instance, int num_employees, int num_vehicles);

void write_output_csvs(const Solution &solution, DARPInstance &instance, std::string god);