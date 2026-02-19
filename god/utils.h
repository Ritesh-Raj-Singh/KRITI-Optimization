#pragma once

#include "model.h"
#include <set>
#include <vector>
#include <string>
#include <utility>
#include <limits>

class DARPInstance
{
public:
    DARPInstance();

    // --- OPTIMIZATION: Use vectors instead of maps for O(1) Lookup ---
    // We assume max_node_id is reasonably small (< 10,000).
    std::vector<Node> nodes; // index = node_id
    std::vector<Vehicle> vehicles;
    std::vector<Request> requests;

    std::vector<std::vector<double>> distance_matrix;
    std::vector<std::vector<double>> time_matrix;

    // Scratchpad for High-Performance Evaluation
    std::vector<double> pickup_times_scratch;
    std::vector<int> pickup_visited_scratch;

    // Route evaluation scratch space (indexed by node_id)
    // Stored here instead of in Route to avoid copying ~20KB/route with Solution
    std::vector<double> scratch_arrival;
    std::vector<double> scratch_service;
    std::vector<double> scratch_waiting;
    std::vector<double> scratch_loads;
    std::vector<double> scratch_ride;
    std::vector<int> node_to_request;   // index = node_id -> returns req_id
    std::vector<int> request_to_pickup; // index = req_id -> returns pickup_node_id
    std::vector<double> req_max_ride;   // index = req_id -> returns max_ride
    std::vector<int> req_id_to_index;   // index = req_id -> returns index into requests[]

    std::vector<std::pair<double, double>> tariff_a;
    std::vector<std::pair<double, double>> tariff_b;

    // Helper to resize everything safely
    void fit_structures(int max_id);
    void set_node(int id, const Node &n); // Helper to assign nodes safely

    // Getter Methods
    // Defined inline for maximum speed (compiler optimization)
    inline double get_dist(int i, int j) const
    {
        return distance_matrix[i][j];
    }
    inline double get_time(int i, int j) const
    {
        return time_matrix[i][j];
    }
    inline int get_req_id_by_node(int node_id) const
    {
        if (node_id < node_to_request.size())
            return node_to_request[node_id];
        return -1;
    }
    inline int get_pickup_node_by_req(int req_id) const
    {
        if (req_id < request_to_pickup.size())
            return request_to_pickup[req_id];
        return -1;
    }
    inline double get_max_ride_time_by_req(int req_id) const
    {
        if (req_id < (int)req_max_ride.size())
            return req_max_ride[req_id];
        return std::numeric_limits<double>::infinity();
    }
    inline const Request &get_request(int req_id) const
    {
        return requests[req_id_to_index[req_id]];
    }

    void register_request(const Request &req);
};

double print_ride_times(const Solution &solution, DARPInstance &instance);