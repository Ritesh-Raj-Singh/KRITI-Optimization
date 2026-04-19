#pragma once
#include <utility>
#include <vector>

#include "evaluate.h"
#include "model.h"
#include "utils.h"

struct RouteBackup {
    int index;
    std::vector<int> sequence;
    double f1, f2, viol_cap, viol_tw, viol_ride;
    double total_distance, total_duration;
    bool is_feasible;
};
using RouteBackups = std::vector<RouteBackup>;

bool neighborhood_move(Solution &solution, int h, DARPInstance &instance, RouteBackups &backups);
bool neighborhood_swap(Solution &solution, int h, DARPInstance &instance, RouteBackups &backups);
bool neighborhood_chain(Solution &solution, int h, DARPInstance &instance, RouteBackups &backups);

void apply_local_search(Solution &solution, DARPInstance &instance);

Solution vns1(const Solution &initial_solution, DARPInstance &instance, long long int k_max, int h,
              double max_time_seconds);