#pragma once
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "params.h"

enum class NodeType { PICKUP, DELIVERY, DEPOT };

class Node {
   public:
    Node();
    Node(int node_id, NodeType node_type, double latitude, double longitude, double start_window, double end_window,
         double service_time, int load_change, int sharing_preference, char waiting_tariff = '\0',
         int employee_priority = 0);

    int id;
    NodeType type;
    double lat;
    double lon;
    double e;
    double l;
    double st;
    int load;
    int sharing_preference;
    char waiting_tariff;
    int priority;

    std::string repr();
};

class Vehicle {
   public:
    Vehicle(int vehicle_id, int depot_node_id, int capacity, double fixed_cost, double unit_length_cost,
            double unit_time_cost, double max_dist_fixed, double cost_per_employee, int vehicle_type_id,
            double average_speed_kmph);

    int id;
    int depot_id;
    int capacity;
    double average_speed;

    double lambda_k;
    double c_k;
    double ct_k;
    double L;
    double cp;

    int type_id;

    std::string repr();
};

class Request {
   public:
    Request(int request_id, const Node &pickup_node, const Node &delivery_node, double max_ride_time,
            uint32_t compatible_vehicle_types);

    int id;
    Node pickup_node;
    Node delivery_node;
    double max_ride_time;
    uint32_t compatible_vehicle_types;
};

class Route {
   public:
    Route(const Vehicle *vehicle);

    const Vehicle *vehicle;
    std::vector<int> sequence;

    double total_distance = 0.0;
    double total_duration = 0.0;
    bool is_feasible = true;

    double f1 = 0.0;
    double f2 = 0.0;

    double viol_cap = 0.0;
    double viol_tw = 0.0;
    double viol_ride = 0.0;

    bool stats_valid = false;

    inline void invalidate_stats() { stats_valid = false; }

    std::string repr();
};

class Solution {
   public:
    Solution();
    Solution(const std::vector<Route> &routes, const std::vector<int> &unassigned_requests);

    std::vector<Route> routes;
    std::vector<int> unassigned_requests;

    double f1 = 0.0;
    double f2 = 0.0;
    double f3 = 0.0;
    double alpha = Params::ALPHA;
    double beta = Params::BETA;
    double gamma = Params::GAMMA;

    double total_cost();

    Solution deep_copy() const;
};
