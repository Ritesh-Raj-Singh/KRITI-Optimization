#pragma once

#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "model.h"

class DARPInstance {
   public:
    DARPInstance();

    std::vector<Node> nodes;
    std::vector<Vehicle> vehicles;
    std::vector<Request> requests;

    std::vector<std::vector<double>> distance_matrix;
    std::vector<std::vector<double>> time_matrix;

    std::vector<double> pickup_times_scratch;
    std::vector<int> pickup_visited_scratch;

    std::vector<double> scratch_arrival;
    std::vector<double> scratch_service;
    std::vector<double> scratch_waiting;
    std::vector<double> scratch_loads;
    std::vector<double> scratch_ride;
    std::vector<int> node_to_request;
    std::vector<int> request_to_pickup;
    std::vector<double> req_max_ride;
    std::vector<int> req_id_to_index;

    std::vector<std::string> vehicle_id_to_original;
    std::vector<std::string> employee_id_to_original;

    std::vector<std::pair<double, double>> tariff_a;
    std::vector<std::pair<double, double>> tariff_b;

    void fit_structures(int max_id);
    void set_node(int id, const Node &n);

    inline double get_dist(int i, int j) const { return distance_matrix[i][j]; }
    inline double get_time(int i, int j) const { return time_matrix[i][j]; }
    inline int get_req_id_by_node(int node_id) const {
        if (node_id < node_to_request.size()) return node_to_request[node_id];
        return -1;
    }
    inline int get_pickup_node_by_req(int req_id) const {
        if (req_id < request_to_pickup.size()) return request_to_pickup[req_id];
        return -1;
    }
    inline double get_max_ride_time_by_req(int req_id) const {
        if (req_id < (int)req_max_ride.size()) return req_max_ride[req_id];
        return std::numeric_limits<double>::infinity();
    }
    inline const Request &get_request(int req_id) const { return requests[req_id_to_index[req_id]]; }

    void register_request(const Request &req);
};

double print_ride_times(const Solution &solution, DARPInstance &instance);