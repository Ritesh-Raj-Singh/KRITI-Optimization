#ifndef FEASIBILITYCHECKER_H
#define FEASIBILITYCHECKER_H
#pragma once

#include <vector>

#include "globals.h"
#include "structures.h"

enum class EvaluationMode { STRICT, PENALTY };

class FeasibilityChecker {
   private:
    const std::vector<Node> &nodes;
    const std::vector<Request> &requests;
    const std::vector<Vehicle> &vehicles;
    int max_global_capacity;

   public:
    FeasibilityChecker(const std::vector<Node> &n, const std::vector<Request> &r, const std::vector<Vehicle> &v,
                       int global_cap, EvaluationMode m = EvaluationMode::PENALTY);

    bool checkInsert(const std::vector<int> &current_route_ids, int vehicle_idx, int pickup_node_id,
                     int delivery_node_id, int pickup_pos, int delivery_pos);

    bool runEightStepEvaluation(const std::vector<int> &route_ids, int veh_idx);

    long long getPenalty() const { return total_penalty; }

    EvaluationMode mode;
    long long total_penalty;

    long long penalty_time_window = tot_speed_per_km * 1000;
    long long penalty_premium_vehicle = 5000000;
    long long penalty_sharing_preference = 5000000;
};

#endif  // FEASIBILITYCHECKER_H