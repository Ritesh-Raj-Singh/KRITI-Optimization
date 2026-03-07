#pragma once
#include <string>
#include "params.h"
#include <cstdint>
#include <list>
#include <vector>
#include <unordered_map>
#include <utility>

// Enum for node types - improves performance by avoiding string comparisons
enum class NodeType
{
    PICKUP,
    DELIVERY,
    DEPOT
};

// Represents a location in the graph (Pickup, Delivery, or Depot).
class Node
{
public:
    Node();
    Node(int node_id, NodeType node_type,
         double latitude, double longitude,
         double start_window, double end_window,
         double service_time, int load_change,
         int sharing_preference, char waiting_tariff = '\0', int employee_priority = 0);

    int id;
    NodeType type; // NodeType enum: PICKUP, DELIVERY, or DEPOT
    double lat;
    double lon;
    double e;               // Start window (e_i)
    double l;               // End window (l_i)
    double st;              // Service time (st_i)
    int load;               // Load change (S_i)
    int sharing_preference; // 1=single, 2=double, 3=triple, 0=N/A (depot)
    char waiting_tariff;    // 'a', 'b', or '\0'
    int priority;           // priority of the employee

    std::string repr();
};

// Represents a heterogeneous vehicle and its cost parameters.
class Vehicle
{
public:
    Vehicle(int vehicle_id, int depot_node_id, int capacity,
            double fixed_cost, double unit_length_cost, double unit_time_cost,
            double max_dist_fixed, double cost_per_employee, int vehicle_type_id, double average_speed_kmph);

    int id;
    int depot_id;         // dep(p)
    int capacity;         // q^p
    double average_speed; // Average speed of the vehicle

    // Cost parameters
    double lambda_k; // Fixed distance cost
    double c_k;      // Unit length cost
    double ct_k;     // Unit time cost
    double L;        // Maximum distance for fixed cost
    double cp;       // Cost per employee transported

    int type_id; // To check compatibility

    std::string repr();
};

// Represents a Transportation Service (i, i+n).
class Request
{
public:
    Request(int request_id, const Node &pickup_node, const Node &delivery_node,
            double max_ride_time, uint32_t compatible_vehicle_types);

    int id;
    Node pickup_node;                  // i
    Node delivery_node;                // i+n
    double max_ride_time;              // T_i
    uint32_t compatible_vehicle_types; // K_i (bitmask: bit N set = type N compatible)
};

// Represents a single vehicle's schedule and timing metrics.
class Route
{
public:
    Route(const Vehicle *vehicle);

    const Vehicle *vehicle;
    std::vector<int> sequence; // List of Node IDs visited

    // Calculated metrics
    double total_distance = 0.0;
    double total_duration = 0.0;
    bool is_feasible = true;

    // Cost components
    double f1 = 0.0;
    double f2 = 0.0;

    // Violations (unweighted)
    double viol_cap = 0.0;
    double viol_tw = 0.0;
    double viol_ride = 0.0;

    // OPTIMIZATION: Cache validity flag
    // When true, the stats (f1, f2, violations, etc.) are valid and don't need recalculation
    // When false, stats must be recalculated
    bool stats_valid = false;

    // Helper to invalidate cache when sequence changes
    inline void invalidate_stats()
    {
        stats_valid = false;
    }

    std::string repr();
};

// Represents a full solution to the DARP.
class Solution
{
public:
    Solution();
    Solution(const std::vector<Route> &routes, const std::vector<int> &unassigned_requests);

    std::vector<Route> routes;
    std::vector<int> unassigned_requests;

    // Objective Function Components
    double f1 = 0.0; // Monetary Cost
    double f2 = 0.0; // Quality of Service
    double f3 = 0.0; // Penalty Cost

    // Dynamic Penalty Coefficients
    double alpha = Params::ALPHA; // Ride Time Violation (α)
    double beta  = Params::BETA;  // Time Window Violation (β)
    double gamma = Params::GAMMA; // Capacity Violation (γ)

    double total_cost();

    // Deep copy helper
    Solution deep_copy() const;
};
