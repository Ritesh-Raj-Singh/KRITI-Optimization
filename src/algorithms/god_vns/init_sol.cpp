#include "init_sol.h"

Solution initial_solution(DARPInstance &instance) {
    std::vector<Route> routes;
    for (const auto &veh : instance.vehicles) routes.emplace_back(&veh);

    std::vector<int> unassigned;
    Solution solution(routes, unassigned);

    std::vector<Request> sorted_requests(instance.requests.begin(), instance.requests.end());

    std::sort(sorted_requests.begin(), sorted_requests.end(),
              [](const Request &a, const Request &b) { return a.delivery_node.l < b.delivery_node.l; });

    for (const auto &req : sorted_requests) {
        double best_overall_cost = std::numeric_limits<double>::infinity();
        Route *best_route_ptr = nullptr;
        std::vector<int> best_seq;

        for (auto &route : solution.routes) {
            if (!(req.compatible_vehicle_types & (1u << route.vehicle->type_id))) continue;

            if (route.vehicle->average_speed <= 0.0) continue;

            auto result =
                insert_request_best_position(route, req, instance, solution.alpha, solution.beta, solution.gamma);

            if (result.first < best_overall_cost) {
                best_overall_cost = result.first;
                best_route_ptr = &route;
                best_seq = std::move(result.second);
            }
        }

        if (best_route_ptr && !best_seq.empty()) {
            best_route_ptr->sequence = std::move(best_seq);
            best_route_ptr->invalidate_stats();
        } else {
            solution.unassigned_requests.push_back(req.id);
        }
    }

    evaluate_solution(solution, instance);

    return solution;
}
