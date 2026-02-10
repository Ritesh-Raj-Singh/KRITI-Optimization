#ifndef FEASIBILITYCHECKER_H
#define FEASIBILITYCHECKER_H
#pragma once
#include "structures.h"
#include <vector>
#include "globals.h"

class FeasibilityChecker
{
private:
    const std::vector<Node> &nodes;
    const std::vector<Request> &requests;
    const std::vector<Vehicle> &vehicles;
    int max_global_capacity;

public:
    FeasibilityChecker(
        const std::vector<Node> &n,
        const std::vector<Request> &r,
        const std::vector<Vehicle> &v,
        int global_cap);

    bool checkInsert(
        const std::vector<int> &current_route_ids,
        int vehicle_idx,
        int pickup_node_id,
        int delivery_node_id,
        int pickup_pos,
        int delivery_pos);

    // MOVED TO PUBLIC for optimization access
    bool runEightStepEvaluation(const std::vector<int> &route_ids, int veh_idx);
};

#endif