#ifndef GRAPHBUILDER_H
#define GRAPHBUILDER_H
#pragma once
#include <vector>

#include "globals.h"
#include "structures.h"

class GraphBuilder {
   public:
    int n_u;
    int n_v;
    int n;
    int max_global_capacity;

    std::vector<Node> nodes;

    GraphBuilder(const std::vector<Request> &requests, const std::vector<Vehicle> &vehicles);

    int getPickupNodeId(int reqIndex);
    int getDeliveryNodeId(int reqIndex);

   private:
    void buildNodes(const std::vector<Request> &requests, const std::vector<Vehicle> &vehicles);
};

#endif