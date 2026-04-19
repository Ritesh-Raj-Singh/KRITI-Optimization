#include "model.h"

#include "params.h"

Node::Node()
    : id(-1),
      type(NodeType::DEPOT),
      lat(0.0),
      lon(0.0),
      e(0.0),
      l(0.0),
      st(0.0),
      load(0),
      sharing_preference(0),
      waiting_tariff('\0') {}

Node::Node(int node_id, NodeType node_type, double latitude, double longitude, double start_window, double end_window,
           double service_time, int load_change, int sharing_preference, char waiting_tariff, int employee_priority)
    : id(node_id),
      type(node_type),
      lat(latitude),
      lon(longitude),
      e(start_window),
      l(end_window),
      st(service_time),
      load(load_change),
      sharing_preference(sharing_preference),
      waiting_tariff(waiting_tariff),
      priority(employee_priority) {}

std::string Node::repr() {
    std::string type_str;
    switch (type) {
        case NodeType::PICKUP:
            type_str = "pickup";
            break;
        case NodeType::DELIVERY:
            type_str = "delivery";
            break;
        case NodeType::DEPOT:
            type_str = "depot";
            break;
    }
    return ("Node(" + std::to_string(id) + ", " + type_str + ")");
}

Vehicle::Vehicle(int vehicle_id, int depot_node_id, int capacity, double fixed_cost, double unit_length_cost,
                 double unit_time_cost, double max_dist_fixed, double cost_per_employee, int vehicle_type_id,
                 double average_speed_kmph)
    : id(vehicle_id),
      depot_id(depot_node_id),
      capacity(capacity),
      lambda_k(fixed_cost),
      c_k(unit_length_cost),
      ct_k(unit_time_cost),
      L(max_dist_fixed),
      cp(cost_per_employee),
      type_id(vehicle_type_id),
      average_speed(average_speed_kmph) {}

std::string Vehicle::repr() { return ("Vehicle(" + std::to_string(id) + ", Cap: " + std::to_string(capacity) + ")"); }

Request::Request(int request_id, const Node &pickup_node, const Node &delivery_node, double max_ride_time,
                 uint32_t compatible_vehicle_types)
    : id(request_id),
      pickup_node(pickup_node),
      delivery_node(delivery_node),
      max_ride_time(max_ride_time),
      compatible_vehicle_types(compatible_vehicle_types) {}

Route::Route(const Vehicle *vehicle) : vehicle(vehicle) { stats_valid = false; }

std::string Route::repr() {
    return ("Route(Vehicle " + std::to_string(vehicle->id) + ", Stops: " + std::to_string(sequence.size()) + ")");
}

Solution::Solution() : f1(0), f2(0), f3(0), alpha(Params::ALPHA), beta(Params::BETA), gamma(Params::GAMMA) {}
Solution::Solution(const std::vector<Route> &routes, const std::vector<int> &unassigned_requests)
    : routes(routes),
      unassigned_requests(unassigned_requests),
      f1(0),
      f2(0),
      f3(0),
      alpha(Params::ALPHA),
      beta(Params::BETA),
      gamma(Params::GAMMA) {}

double Solution::total_cost() { return f1 + f2 + f3; }

Solution Solution::deep_copy() const { return Solution(*this); }
