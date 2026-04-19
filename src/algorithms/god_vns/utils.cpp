#include "utils.h"

#include <algorithm>
#include <iostream>

DARPInstance::DARPInstance() {}

void DARPInstance::fit_structures(int max_id) {
    int size = max_id + 1;
    if ((int)nodes.size() < size) nodes.resize(size);
    if ((int)node_to_request.size() < size) node_to_request.resize(size, -1);
    if ((int)request_to_pickup.size() < size) request_to_pickup.resize(size, -1);
    if ((int)req_max_ride.size() < size) req_max_ride.resize(size, 0.0);

    if ((int)distance_matrix.size() < size) {
        distance_matrix.resize(size, std::vector<double>(size, std::numeric_limits<double>::infinity()));
        for (auto &row : distance_matrix) row.resize(size, std::numeric_limits<double>::infinity());
    }
    if ((int)time_matrix.size() < size) {
        time_matrix.resize(size, std::vector<double>(size, std::numeric_limits<double>::infinity()));
        for (auto &row : time_matrix) row.resize(size, std::numeric_limits<double>::infinity());
    }

    if ((int)pickup_times_scratch.size() < size) pickup_times_scratch.resize(size, -1.0);
    pickup_visited_scratch.reserve(size);

    if ((int)scratch_arrival.size() < size) scratch_arrival.resize(size, 0.0);
    if ((int)scratch_service.size() < size) scratch_service.resize(size, 0.0);
    if ((int)scratch_waiting.size() < size) scratch_waiting.resize(size, 0.0);
    if ((int)scratch_loads.size() < size) scratch_loads.resize(size, 0.0);
    if ((int)scratch_ride.size() < size) scratch_ride.resize(size, 0.0);
}

void DARPInstance::set_node(int id, const Node &n) {
    fit_structures(id);
    nodes[id] = n;
}

void DARPInstance::register_request(const Request &req) {
    requests.push_back(req);

    int max_req_id = req.id;
    if (request_to_pickup.size() <= max_req_id) request_to_pickup.resize(max_req_id + 1, -1);
    if (req_max_ride.size() <= max_req_id) req_max_ride.resize(max_req_id + 1, 0.0);

    fit_structures(std::max(req.pickup_node.id, req.delivery_node.id));

    node_to_request[req.pickup_node.id] = req.id;
    node_to_request[req.delivery_node.id] = req.id;
    request_to_pickup[req.id] = req.pickup_node.id;
    req_max_ride[req.id] = req.max_ride_time;

    int idx = static_cast<int>(requests.size()) - 1;
    if ((int)req_id_to_index.size() <= req.id) req_id_to_index.resize(req.id + 1, -1);
    req_id_to_index[req.id] = idx;
}

double print_ride_times(const Solution &solution, DARPInstance &instance) {
    std::cout << "\n--- Employee Ride Times ---\n";

    double total_ride_time = 0.0;

    std::vector<double> &pickup_times = instance.pickup_times_scratch;
    std::vector<int> &pickup_visited = instance.pickup_visited_scratch;

    for (const Route &route : solution.routes) {
        if (route.sequence.empty()) continue;

        pickup_visited.clear();

        const Vehicle &vehicle = *route.vehicle;
        double speed = vehicle.average_speed;

        int prev_node_id = vehicle.depot_id;
        const Node *prev_node = &instance.nodes[prev_node_id];

        const Node *first_node = &instance.nodes[route.sequence.front()];
        double dist_to_first = instance.get_dist(prev_node_id, first_node->id);
        double start_time = std::max(0.0, first_node->e - (dist_to_first / speed) * 60.0);

        double current_time = start_time;

        for (int node_id : route.sequence) {
            const Node &node = instance.nodes[node_id];

            double dist = instance.get_dist(prev_node_id, node.id);
            double travel_time = (dist / speed) * 60.0;
            double arrival_time = current_time + prev_node->st + travel_time;
            double start_service_time = std::max(node.e, arrival_time);

            if (node.type == NodeType::PICKUP) {
                pickup_times[node.id] = start_service_time;
                pickup_visited.push_back(node.id);
            } else if (node.type == NodeType::DELIVERY) {
                int req_id = instance.get_req_id_by_node(node.id);
                int pickup_node_id = instance.get_pickup_node_by_req(req_id);
                if (pickup_times[pickup_node_id] != -1.0) {
                    double ride =
                        start_service_time - (pickup_times[pickup_node_id] + instance.nodes[pickup_node_id].st);
                    std::cout << "Request " << req_id << ": " << ride << " min\n";
                    total_ride_time += ride;
                }
            }

            current_time = start_service_time;
            prev_node = &node;
            prev_node_id = node.id;
        }

        for (int id : pickup_visited) pickup_times[id] = -1.0;
    }

    return total_ride_time;
}