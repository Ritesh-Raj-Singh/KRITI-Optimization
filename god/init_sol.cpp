#include "init_sol.h"

Solution initial_solution(DARPInstance &instance)
{
    // 1. Initialize empty routes (one per vehicle)
    std::vector<Route> routes;
    for (const auto &veh : instance.vehicles)
        routes.emplace_back(&veh);

    std::vector<int> unassigned;
    Solution solution(routes, unassigned);

    // 2. Sort requests by delivery time window (l_i)
    std::vector<Request> sorted_requests(
        instance.requests.begin(), instance.requests.end());

    std::sort(sorted_requests.begin(), sorted_requests.end(),
              [](const Request &a, const Request &b)
              {
                  return a.delivery_node.l < b.delivery_node.l;
              });

    // 3. Process each request
    for (const auto &req : sorted_requests)
    {
        bool assigned = false;

        /* ===============================
           1. Try empty routes
           =============================== */
        Route *best_empty_route = nullptr;
        double min_empty_dist = std::numeric_limits<double>::infinity();

        for (auto &route : solution.routes)
        {
            if (!route.sequence.empty())
                continue;

            if (!(req.compatible_vehicle_types & (1u << route.vehicle->type_id)))
                continue;

            int depot_id = route.vehicle->depot_id;
            double d = instance.get_dist(depot_id, req.pickup_node.id);

            if (d < min_empty_dist)
            {
                min_empty_dist = d;
                best_empty_route = &route;
            }
        }

        if (best_empty_route)
        {
            best_empty_route->sequence.push_back(req.pickup_node.id);
            best_empty_route->sequence.push_back(req.delivery_node.id);
            assigned = true;
        }

        /* ===============================
           2. Try non-empty routes
           =============================== */
        if (!assigned)
        {
            Route *best_route = nullptr;
            double min_dist = std::numeric_limits<double>::infinity();

            for (auto &route : solution.routes)
            {
                if (route.sequence.empty())
                    continue;

                if (!(req.compatible_vehicle_types & (1u << route.vehicle->type_id)))
                    continue;

                int last_node = route.sequence.back();
                double d = instance.get_dist(last_node, req.pickup_node.id);

                if (d < min_dist)
                {
                    min_dist = d;
                    best_route = &route;
                }
            }

            if (best_route)
            {
                best_route->sequence.push_back(req.pickup_node.id);
                best_route->sequence.push_back(req.delivery_node.id);
                assigned = true;
            }
        }

        /* ===============================
           3. Forced insertion fallback
           =============================== */
        if (!assigned)
        {
            double best_cost = std::numeric_limits<double>::infinity();
            Route *best_route = nullptr;
            std::vector<int> best_sequence;

            for (auto &route : solution.routes)
            {
                if (!(req.compatible_vehicle_types & (1u << route.vehicle->type_id)))
                    continue;

                // Placeholder: you will implement this
                auto result = insert_request_best_position(
                    route, req, instance,
                    solution.alpha, solution.beta, solution.gamma);

                double cost = result.first;
                const std::vector<int> &seq = result.second;

                if (cost < best_cost)
                {
                    best_cost = cost;
                    best_route = &route;
                    best_sequence = seq;
                }
            }

            if (best_route)
            {
                best_route->sequence = best_sequence;
                assigned = true;
            }
        }

        /* ===============================
           4. Unassigned
           =============================== */
        if (!assigned)
        {
            solution.unassigned_requests.push_back(req.id);
        }
    }

    // 4. Final evaluation
    evaluate_solution(solution, instance);

    return solution;
}
