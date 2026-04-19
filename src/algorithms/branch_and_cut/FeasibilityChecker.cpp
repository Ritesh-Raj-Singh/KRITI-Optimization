#include "FeasibilityChecker.h"

#include <algorithm>

#include "matrix.h"
#include "utils.h"

FeasibilityChecker::FeasibilityChecker(const std::vector<Node> &n, const std::vector<Request> &r,
                                       const std::vector<Vehicle> &v, int global_cap, EvaluationMode m)
    : nodes(n), requests(r), vehicles(v), max_global_capacity(global_cap), mode(m), total_penalty(0) {}

bool FeasibilityChecker::checkInsert(const std::vector<int> &current_route_ids, int vehicle_idx, int pickup_node_id,
                                     int delivery_node_id, int pickup_pos, int delivery_pos) {
    std::vector<int> test_route = current_route_ids;
    test_route.insert(test_route.begin() + delivery_pos, delivery_node_id);
    test_route.insert(test_route.begin() + pickup_pos, pickup_node_id);
    return runEightStepEvaluation(test_route, vehicle_idx);
}

bool FeasibilityChecker::runEightStepEvaluation(const std::vector<int> &route_ids, int veh_idx) {
    total_penalty = 0;

    int U = route_ids.size();
    if (U == 0) return true;

    const Vehicle &vehicle = vehicles[veh_idx];

    std::vector<int> A(U), D(U), W(U), L(U);

    int start_node = route_ids[0];
    A[0] = nodes[start_node].earliest_time;
    D[0] = A[0];
    L[0] = nodes[start_node].demand;

    std::vector<int> active_requests;

    std::vector<bool> sharing_penalized(requests.size(), false);

    for (int i = 1; i < U; ++i) {
        int prev = route_ids[i - 1];
        int curr = route_ids[i];
        const Node &n_curr = nodes[curr];
        const Node &n_prev = nodes[prev];

        int travel = 0;
        if (n_curr.type != Node::DUMMY_END) {
            travel = getTravelTimeByIndex(n_prev.getMatrixIndex(), n_curr.getMatrixIndex(), vehicle.avg_speed_kmh);
        }

        A[i] = D[i - 1] + travel;
        W[i] = std::max(0, n_curr.earliest_time - A[i]);
        int B_i = A[i] + W[i];

        if (B_i > n_curr.latest_time) {
            if (mode == EvaluationMode::STRICT) {
                return false;
            } else {
                long long violation = B_i - n_curr.latest_time;
                total_penalty += violation * penalty_time_window;
            }
        }

        D[i] = B_i + n_curr.service_duration;
        L[i] = L[i - 1] + n_curr.demand;

        if (L[i] > max_global_capacity) {
            return false;
        }

        if (n_curr.type == Node::PICKUP) {
            const Request &r = requests[n_curr.request_id];

            if (!r.isVehicleCompatible(vehicle.category)) {
                if (mode == EvaluationMode::STRICT) {
                    return false;
                } else {
                    total_penalty += penalty_premium_vehicle;
                }
            }
            active_requests.push_back(n_curr.request_id);
        } else if (n_curr.type == Node::DELIVERY) {
            auto it = std::remove(active_requests.begin(), active_requests.end(), n_curr.request_id);
            active_requests.erase(it, active_requests.end());
        }

        int dummy_load = max_global_capacity - vehicle.max_capacity;
        int current_real_load = L[i] - dummy_load;

        for (int req_id : active_requests) {
            const Request &r = requests[req_id];
            int max_passengers_allowed = 1 + r.max_shared_with;

            if (current_real_load > max_passengers_allowed) {
                if (mode == EvaluationMode::STRICT) {
                    return false;
                } else {
                    if (!sharing_penalized[req_id]) {
                        total_penalty += penalty_sharing_preference;
                        sharing_penalized[req_id] = true;
                    }
                }
            }
        }
    }

    std::vector<int> F(U);
    F[U - 1] = nodes[route_ids[U - 1]].latest_time - A[U - 1];
    for (int i = U - 2; i >= 0; --i) {
        const Node &n_curr = nodes[route_ids[i]];
        F[i] = std::min(n_curr.latest_time - (A[i] + W[i]), F[i + 1] + W[i + 1]);
    }

    return true;
}