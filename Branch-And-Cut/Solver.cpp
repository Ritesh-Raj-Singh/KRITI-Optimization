#include "Solver.h"
#include "utils.h"
#include <limits>
#include <algorithm>
#include <random>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <map>
#include <unordered_map>
#include <functional>
#include <queue>
// std::mt19937_64 RNG(std::chrono::steady_clock::now().time_since_epoch().count());

Solver::Solver(const std::vector<Request> &r, const std::vector<Vehicle> &v, GraphBuilder &gb)
    : requests(r), vehicles(v), graph(gb),
      checker(gb.nodes, r, v, gb.max_global_capacity, EvaluationMode::PENALTY) {}

long long Solver::getRoutePenalty(const std::vector<int> &route, int veh_idx)
{
    if (checker.runEightStepEvaluation(route, veh_idx))
    {
        return checker.getPenalty();
    }
    return 1e15;
}

// --- COST CALCULATION ---
double Solver::calculateRouteCost(const std::vector<int> &route_ids, const Vehicle &v)
{
    if (route_ids.empty())
        return 0.0;
    double total_dist = 0.0;
    double current_time = std::max((double)v.available_from, (double)graph.nodes[route_ids[0]].earliest_time);
    // double start_time = current_time;

    double total_passenger_travel_time = 0.0;
    std::map<int, double> passenger_boarding_times;

    for (size_t i = 0; i < route_ids.size() - 1; ++i)
    {
        int u = route_ids[i];
        int next = route_ids[i + 1];
        const Node &n_u = graph.nodes[u];
        const Node &n_next = graph.nodes[next];

        // Service time
        double arrival = std::max((double)n_u.earliest_time, current_time);
        double departure = std::max((double)n_u.earliest_time, current_time) + n_u.service_duration;

        if (n_u.type == Node::PICKUP)
        {
            passenger_boarding_times[n_u.request_id] = arrival;
        }
        else if (n_u.type == Node::DELIVERY)
        {
            if (passenger_boarding_times.find(n_u.request_id) != passenger_boarding_times.end())
            {
                double ride_time = arrival - passenger_boarding_times[n_u.request_id];
                total_passenger_travel_time += ride_time;
            }
        }

        double dist = 0, time = 0;
        if (n_next.type != Node::DUMMY_END)
        {
            dist = getDistanceByIndex(n_u.getMatrixIndex(), n_next.getMatrixIndex());
            double speed = (v.avg_speed_kmh > 0) ? v.avg_speed_kmh : 30.0;
            time = (dist / speed) * 60.0;
        }

        total_dist += dist;
        current_time = departure + time;
    }

    // double duration = current_time - start_time;
    double monetary_cost = total_dist * v.cost_per_km;
    // Note: ensure weighted_dist_cost and weight_time_cost are defined in Solver.h or locally
    return (dist_cost * monetary_cost) + (time_cost * total_passenger_travel_time);
}

double Solver::calculateTotalCost(const Solution &sol)
{
    double total_objective = 0;

    for (auto const &[vid, route] : sol.routes)
    {
        int v_idx = vid - 1;

        // 1. Physical Cost
        double physical_cost = calculateRouteCost(route, vehicles[v_idx]);

        // 2. Penalty Cost
        long long penalty = 0;
        // Check feasibility (EvaluationMode::PENALTY should be set in constructor)
        if (checker.runEightStepEvaluation(route, v_idx))
        {
            penalty = checker.getPenalty();
        }
        else
        {
            // Hard constraint violation (Capacity/Compatibility)
            // Apply massive penalty to discourage this state
            penalty = 1e9;
        }

        total_objective += (physical_cost + penalty);
    }
    return total_objective;
}

//--- INITIAL SOLUTION (Greedy Insertion) ---
void Solver::buildInitialSolution(Solution &sol)
{
    // Init empty routes
    for (const auto &veh : vehicles)
    {
        sol.routes[veh.id] = {1 + (veh.id - 1), graph.n + 1 + (veh.id - 1)};
    }

    // Sort requests by Earliest Pickup Time (simple heuristic)
    std::vector<int> sorted_reqs(requests.size());
    for (int i = 0; i < (int)requests.size(); ++i)
        sorted_reqs[i] = i;
    // std::sort(sorted_reqs.begin(), sorted_reqs.end(), [&](int a, int b)
    //           { return requests[a].earliest_pickup < requests[b].earliest_pickup; });

    static std::mt19937 g(std::time(nullptr));
    std::shuffle(sorted_reqs.begin(), sorted_reqs.end(), g);

    for (int req_idx : sorted_reqs)
    {
        if (!insertRequestBest(sol, req_idx))
        {
            sol.unserved_requests.push_back(req_idx);
        }
    }
    sol.total_cost = calculateTotalCost(sol);
}

// --- REGRET-BASED INITIAL SOLUTION ---
// Replaces the simple greedy insertion.
// Complexity: O(N^2 * V * L) - Slower to start, but MUCH better results.
void Solver::buildRegretSolution(Solution &sol)
{
    // 1. Initialize empty routes
    for (const auto &veh : vehicles)
    {
        sol.routes[veh.id] = {1 + (veh.id - 1), graph.n + 1 + (veh.id - 1)};
    }

    std::vector<int> unserved = sol.unserved_requests; // Initially all requests are here (if any)
    // Actually, let's just reset and assume all requests need insertion
    std::vector<int> pending_reqs;
    for (int i = 0; i < (int)requests.size(); ++i)
        pending_reqs.push_back(i);

    sol.unserved_requests.clear();

    while (!pending_reqs.empty())
    {
        int best_req_to_insert = -1;
        double max_regret = -1.0;

        struct InsertionInfo
        {
            int veh_id;
            int p_pos;
            int d_pos;
            double cost;
        };

        InsertionInfo global_best_move = {-1, -1, -1, 1e9};

        // Evaluate every pending request
        for (int req_idx : pending_reqs)
        {

            // Find Best and Second Best insertion costs for this specific request
            double best_cost = 1e9;
            double second_best_cost = 1e9;
            InsertionInfo current_req_best_move = {-1, -1, -1, 1e9};

            int p_node = graph.getPickupNodeId(req_idx);
            int d_node = graph.getDeliveryNodeId(req_idx);

            // Check all vehicles
            for (auto &[veh_id, route] : sol.routes)
            {
                int v_idx = veh_id - 1;
                if (!requests[req_idx].isVehicleCompatible(vehicles[v_idx].category))
                    continue;

                double current_route_cost = calculateRouteCost(route, vehicles[v_idx]);

                // Check all positions
                for (int i = 1; i < (int)route.size(); ++i)
                {
                    for (int j = i; j < (int)route.size(); ++j)
                    {
                        if (checker.checkInsert(route, v_idx, p_node, d_node, i, j))
                        {
                            // Calculate Cost
                            std::vector<int> temp_route = route;
                            temp_route.insert(temp_route.begin() + j, d_node);
                            temp_route.insert(temp_route.begin() + i, p_node);

                            double new_cost = calculateRouteCost(temp_route, vehicles[v_idx]);
                            double added_cost = new_cost - current_route_cost;

                            if (added_cost < best_cost)
                            {
                                second_best_cost = best_cost;
                                best_cost = added_cost;
                                current_req_best_move = {veh_id, i, j, added_cost};
                            }
                            else if (added_cost < second_best_cost)
                            {
                                second_best_cost = added_cost;
                            }
                        }
                    }
                }
            }

            // Calculate Regret (Difference between 2nd best and best)
            // If only one option exists, regret is infinite (must pick this one)
            double regret = second_best_cost - best_cost;
            if (second_best_cost >= 1e8 && best_cost < 1e8)
                regret = 1e9;

            if (best_cost >= 1e8)
            {
                // Cannot be inserted anywhere currently
                continue;
            }

            // Maximize Regret
            if (regret > max_regret)
            {
                max_regret = regret;
                best_req_to_insert = req_idx;
                global_best_move = current_req_best_move;
            }
        }

        if (best_req_to_insert != -1)
        {
            // Perform the best move for the high-regret request
            auto &route = sol.routes[global_best_move.veh_id];
            int p_node = graph.getPickupNodeId(best_req_to_insert);
            int d_node = graph.getDeliveryNodeId(best_req_to_insert);

            route.insert(route.begin() + global_best_move.d_pos, d_node);
            route.insert(route.begin() + global_best_move.p_pos, p_node);

            // Remove from pending
            pending_reqs.erase(std::remove(pending_reqs.begin(), pending_reqs.end(), best_req_to_insert), pending_reqs.end());
        }
        else
        {
            // No valid moves for remaining requests
            for (int r : pending_reqs)
                sol.unserved_requests.push_back(r);
            break;
        }
    }

    sol.total_cost = calculateTotalCost(sol);
}

// --- HELPER: BEST INSERTION ---
// Finds the best vehicle and position for a request.
bool Solver::insertRequestBest(Solution &sol, int req_idx, int forbidden_veh_id)
{
    int best_veh_id = -1;
    double best_cost_increase = std::numeric_limits<double>::max();
    int best_p_pos = -1;
    int best_d_pos = -1;

    int p_node = graph.getPickupNodeId(req_idx);
    int d_node = graph.getDeliveryNodeId(req_idx);

    // Iterate all vehicles
    for (auto &[veh_id, route] : sol.routes)
    {
        if (veh_id == forbidden_veh_id)
            continue; // Skip forbidden vehicle (for Elimination op)

        int veh_idx = veh_id - 1;
        if (!requests[req_idx].isVehicleCompatible(vehicles[veh_idx].category))
            continue;

        double current_route_cost = calculateRouteCost(route, vehicles[veh_idx]);
        long long old_penalty = getRoutePenalty(route, veh_idx);

        // Check all positions i (pickup) and j (delivery)
        for (int i = 1; i < (int)route.size(); ++i)
        {
            for (int j = i; j < (int)route.size(); ++j)
            {
                if (checker.checkInsert(route, veh_idx, p_node, d_node, i, j))
                {
                    // Simulate insertion
                    std::vector<int> temp_route = route;
                    temp_route.insert(temp_route.begin() + j, d_node);
                    temp_route.insert(temp_route.begin() + i, p_node);

                    long long new_penalty = checker.getPenalty();

                    double new_cost = calculateRouteCost(temp_route, vehicles[veh_idx]);
                    long long penalty_diff = new_penalty - old_penalty;

                    double increase = new_cost - current_route_cost;

                    if (increase + penalty_diff < best_cost_increase)
                    {
                        best_cost_increase = increase + penalty_diff;
                        best_veh_id = veh_id;
                        best_p_pos = i;
                        best_d_pos = j;
                    }
                }
            }
        }
    }

    // Perform insertion if a valid spot was found
    if (best_veh_id != -1)
    {
        auto &route = sol.routes[best_veh_id];
        route.insert(route.begin() + best_d_pos, d_node);
        route.insert(route.begin() + best_p_pos, p_node);
        return true;
    }

    return false;
}

// Wrapper for when no vehicle is forbidden (standard insertion)
bool Solver::insertRequestBest(Solution &sol, int req_idx)
{
    return insertRequestBest(sol, req_idx, -1);
}

// --- DETERMINISTIC ANNEALING MAIN LOOP ---
Solver::Solution Solver::solveDeterministicAnnealing()
{
    auto begin = std::chrono::high_resolution_clock::now();

    Solution current_sol;
    buildInitialSolution(current_sol);
    // buildGraphMatchingInitialSolution(current_sol);
    // buildRegretSolution(current_sol);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
    std::cerr << "Initial Solution: " << elapsed.count() * 1e-9 << " seconds.\n";

    Solution best_sol = current_sol;

    double avg_cost = calculateAverageEdgeCost();
    double t_max = avg_cost * param_t_max_mult;
    double T = t_max;

    double initial_t = avg_cost * 0.5;
    double TT = initial_t;
    // double TT = 100.0;
    double FT = 0.05;

    int iter_per_temp = 10;
    double total_cooling_steps = (double)max_iterations / iter_per_temp;

    double alpha = std::pow(FT / initial_t, 1.0 / total_cooling_steps);
    // double alpha = 1;
    int reheat_interval = 2000;

    int no_improve_global = 0;

    std::cout << "Starting DA. Init Cost: " << current_sol.total_cost << " | T_max: " << t_max << "\n";

    int no_improve_iter = 0;

    for (int iter = 0; iter < max_iterations; ++iter)
    {
        no_improve_iter++;

        bool accepted = false;

        if (!current_sol.unserved_requests.empty() && (rand() % 100 < 20))
        {
            if (operatorInsertUnserved(current_sol))
            {
                accepted = true;
            }
        }

        if (!accepted)
        {
            int op = rand() % 100;

            if (op < 10)
            {
                accepted = operatorEliminate(current_sol);
            }
            else if (op < 40)
            {
                accepted = operator2Opt(current_sol, T);
            }
            else if (op < 70)
            {
                accepted = operatorExchange(current_sol, T);
            }
            else
            {
                accepted = operatorRelocate(current_sol, T);
            }
        }

        if (accepted)
        {
            if (current_sol.unserved_requests.size() < best_sol.unserved_requests.size())
            {
                best_sol = current_sol;
                no_improve_iter = 0;
                no_improve_global = 0;
            }
            else if (current_sol.unserved_requests.size() == best_sol.unserved_requests.size())
            {
                if (current_sol.total_cost < best_sol.total_cost - 0.001)
                {
                    best_sol = current_sol;
                    no_improve_iter = 0;
                    no_improve_global = 0;
                }
            }
        }

        // if (no_improve_iter > 0)
        // {
        //     T = T - (t_max / param_t_red);

        //     if (T < 0)
        //     {
        //         if (no_improve_iter > param_n_imp)
        //         {
        //             current_sol = best_sol;
        //             no_improve_iter = 0;
        //             double r = ((double)rand() / (RAND_MAX));
        //             T = r * t_max;
        //         }
        //         else
        //         {
        //             double r = ((double)rand() / (RAND_MAX));
        //             T = r * t_max;
        //         }
        //     }
        // }

        if (!accepted)
            no_improve_global++;
        if (iter % iter_per_temp == 0)
        {
            TT *= alpha;
            if (TT < FT)
                TT = FT;
        }
        if (no_improve_global > reheat_interval)
        {
            current_sol = best_sol;
            TT = initial_t * 0.15;
            // TT = initial_t;
            no_improve_global = 0;
        }
    }

    return best_sol;
}

// --- OPERATOR 1: RELOCATE (Inter-Route) ---
bool Solver::operatorRelocate(Solution &sol, double threshold)
{
    std::vector<int> nonEmptyVehicles;
    for (auto const &[vid, r] : sol.routes)
        if (r.size() > 2)
            nonEmptyVehicles.push_back(vid);

    if (nonEmptyVehicles.empty())
        return false;

    int src_veh_id = nonEmptyVehicles[rand() % nonEmptyVehicles.size()];
    std::vector<int> &src_route = sol.routes[src_veh_id];
    int src_idx = src_veh_id - 1;

    std::vector<int> reqs_in_route;
    for (int node : src_route)
        if (graph.nodes[node].type == Node::PICKUP)
            reqs_in_route.push_back(graph.nodes[node].request_id);

    if (reqs_in_route.empty())
        return false;
    int req_idx = reqs_in_route[rand() % reqs_in_route.size()];

    int p_id = graph.getPickupNodeId(req_idx);
    int d_id = graph.getDeliveryNodeId(req_idx);

    std::vector<int> new_src_route;
    for (int node : src_route)
        if (node != p_id && node != d_id)
            new_src_route.push_back(node);

    double cost_old_src = calculateRouteCost(src_route, vehicles[src_veh_id - 1]);
    long long pen_old_src = getRoutePenalty(src_route, src_idx);
    double cost_new_src = calculateRouteCost(new_src_route, vehicles[src_veh_id - 1]);
    long long pen_new_src = getRoutePenalty(new_src_route, src_idx);

    double savings = cost_old_src - cost_new_src + (pen_old_src - pen_new_src);

    int best_tgt_veh = -1;
    std::vector<int> best_tgt_route;
    double best_delta = std::numeric_limits<double>::max();

    for (auto &[tgt_veh_id, tgt_route] : sol.routes)
    {
        if (tgt_veh_id == src_veh_id)
            continue;
        int tgt_idx = tgt_veh_id - 1;
        const Vehicle &v = vehicles[tgt_veh_id - 1];
        if (!requests[req_idx].isVehicleCompatible(v.category))
            continue;

        double cost_old_tgt = calculateRouteCost(tgt_route, v);
        long long pen_old_tgt = getRoutePenalty(tgt_route, tgt_idx);

        for (int i = 1; i < (int)tgt_route.size(); ++i)
        {
            for (int j = i; j < (int)tgt_route.size(); ++j)
            {
                if (checker.checkInsert(tgt_route, tgt_veh_id - 1, p_id, d_id, i, j))
                {
                    long long pen_new_tgt = checker.getPenalty();

                    std::vector<int> temp = tgt_route;
                    temp.insert(temp.begin() + j, d_id);
                    temp.insert(temp.begin() + i, p_id);

                    double cost_new_tgt = calculateRouteCost(temp, v);
                    double insertion_cost = cost_new_tgt - cost_old_tgt + (pen_new_tgt - pen_old_tgt);
                    double total_delta = insertion_cost - savings;

                    if (total_delta < best_delta)
                    {
                        best_delta = total_delta;
                        best_tgt_veh = tgt_veh_id;
                        best_tgt_route = temp;
                    }
                }
            }
        }
    }

    if (best_tgt_veh != -1 && best_delta < threshold)
    {
        sol.routes[src_veh_id] = new_src_route;
        sol.routes[best_tgt_veh] = best_tgt_route;
        sol.total_cost += best_delta;
        return true;
    }

    return false;
}

// --- OPERATOR 2: EXCHANGE (Inter-Route) ---
bool Solver::operatorExchange(Solution &sol, double threshold)
{
    std::vector<int> active;
    for (auto &[id, r] : sol.routes)
        if (r.size() > 2)
            active.push_back(id);
    if (active.size() < 2)
        return false;

    int v1 = active[rand() % active.size()];
    int v2 = active[rand() % active.size()];
    while (v1 == v2)
        v2 = active[rand() % active.size()];

    auto getReq = [&](const std::vector<int> &r)
    {
        std::vector<int> reqs;
        for (int n : r)
            if (graph.nodes[n].type == Node::PICKUP)
                reqs.push_back(graph.nodes[n].request_id);
        return reqs[rand() % reqs.size()];
    };

    int r1_idx = getReq(sol.routes[v1]);
    int r2_idx = getReq(sol.routes[v2]);

    int v1_idx = v1 - 1;
    int v2_idx = v2 - 1;

    if (!requests[r1_idx].isVehicleCompatible(vehicles[v2 - 1].category))
        return false;
    if (!requests[r2_idx].isVehicleCompatible(vehicles[v1 - 1].category))
        return false;

    double cost_v1_old = calculateRouteCost(sol.routes[v1], vehicles[v1_idx]);
    long long pen_v1_old = getRoutePenalty(sol.routes[v1], v1_idx);

    double cost_v2_old = calculateRouteCost(sol.routes[v2], vehicles[v2_idx]);
    long long pen_v2_old = getRoutePenalty(sol.routes[v2], v2_idx);

    if (pen_v1_old >= 1e15 || pen_v2_old >= 1e15)
        return false;

    double old_total_metric = (cost_v1_old + pen_v1_old) + (cost_v2_old + pen_v2_old);

    auto removeReq = [&](std::vector<int> route, int req_id)
    {
        std::vector<int> clean;
        int p = graph.getPickupNodeId(req_id);
        int d = graph.getDeliveryNodeId(req_id);
        for (int n : route)
            if (n != p && n != d)
                clean.push_back(n);
        return clean;
    };

    std::vector<int> temp_v1 = removeReq(sol.routes[v1], r1_idx);
    std::vector<int> temp_v2 = removeReq(sol.routes[v2], r2_idx);

    int p2 = graph.getPickupNodeId(r2_idx), d2 = graph.getDeliveryNodeId(r2_idx);
    int p1 = graph.getPickupNodeId(r1_idx), d1 = graph.getDeliveryNodeId(r1_idx);

    // Find best insertion for r2 into v1
    double best_v1_new_cost = std::numeric_limits<double>::max();
    std::vector<int> best_v1_route;
    for (int i = 1; i < (int)temp_v1.size(); ++i)
    {
        for (int j = i; j < (int)temp_v1.size(); ++j)
        {
            if (checker.checkInsert(temp_v1, v1 - 1, p2, d2, i, j))
            {
                long long current_pen = checker.getPenalty();

                std::vector<int> t = temp_v1;
                t.insert(t.begin() + j, d2);
                t.insert(t.begin() + i, p2);
                double c = calculateRouteCost(t, vehicles[v1 - 1]);
                double metric = current_pen + c;
                if (metric < best_v1_new_cost)
                {
                    best_v1_new_cost = metric;
                    best_v1_route = t;
                }
            }
        }
    }
    if (best_v1_route.empty())
        return false;

    // Find best insertion for r1 into v2
    double best_v2_new_cost = std::numeric_limits<double>::max();
    std::vector<int> best_v2_route;
    for (int i = 1; i < (int)temp_v2.size(); ++i)
    {
        for (int j = i; j < (int)temp_v2.size(); ++j)
        {
            if (checker.checkInsert(temp_v2, v2 - 1, p1, d1, i, j))
            {
                long long current_pen = checker.getPenalty();
                std::vector<int> t = temp_v2;
                t.insert(t.begin() + j, d1);
                t.insert(t.begin() + i, p1);
                double c = calculateRouteCost(t, vehicles[v2 - 1]);
                double metric = c + current_pen;
                if (metric < best_v2_new_cost)
                {
                    best_v2_new_cost = metric;
                    best_v2_route = t;
                }
            }
        }
    }
    if (best_v2_route.empty())
        return false;

    double delta = (best_v1_new_cost + best_v2_new_cost) - (cost_v1_old + cost_v2_old);

    if (delta < threshold)
    {
        sol.routes[v1] = best_v1_route;
        sol.routes[v2] = best_v2_route;
        sol.total_cost += delta;
        return true;
    }
    return false;
}

// --- OPERATOR 3: ELIMINATE (Reduce Vehicles) ---
bool Solver::operatorEliminate(Solution &sol)
{
    std::vector<int> candidate_vehicles;
    for (auto const &[vid, route] : sol.routes)
        if (route.size() > 2)
            candidate_vehicles.push_back(vid);

    if (candidate_vehicles.empty())
        return false;

    int victim_veh_id = candidate_vehicles[rand() % candidate_vehicles.size()];
    std::vector<int> victim_route = sol.routes[victim_veh_id];

    std::vector<int> orphaned_requests;
    for (int node_id : victim_route)
        if (graph.nodes[node_id].type == Node::PICKUP)
            orphaned_requests.push_back(graph.nodes[node_id].request_id);

    Solution backup_sol = sol;

    Vehicle &v = vehicles[victim_veh_id - 1];
    sol.routes[victim_veh_id] = {1 + (v.id - 1), graph.n + 1 + (v.id - 1)};

    static std::mt19937 g(std::time(nullptr));
    std::shuffle(orphaned_requests.begin(), orphaned_requests.end(), g);

    bool success = true;
    for (int req_idx : orphaned_requests)
    {
        // Try to insert into ANY vehicle EXCEPT the victim
        if (!insertRequestBest(sol, req_idx, victim_veh_id))
        {
            success = false;
            break;
        }
    }

    if (success)
    {
        sol.total_cost = calculateTotalCost(sol);
        return true;
    }
    else
    {
        sol = backup_sol;
        return false;
    }
}

// --- OPERATOR 4: 2-OPT (Intra/Inter-Route Optimization) ---
bool Solver::operator2Opt(Solution &sol, double threshold)
{
    std::vector<int> active_vehs;
    for (auto &[id, r] : sol.routes)
        if (r.size() > 2)
            active_vehs.push_back(id);
    if (active_vehs.size() < 2)
        return false;

    int v1_id = active_vehs[rand() % active_vehs.size()];
    int v2_id = active_vehs[rand() % active_vehs.size()];
    while (v1_id == v2_id)
        v2_id = active_vehs[rand() % active_vehs.size()];

    std::vector<int> &r1 = sol.routes[v1_id];
    std::vector<int> &r2 = sol.routes[v2_id];

    auto getEmptyPoints = [&](const std::vector<int> &r, int veh_idx)
    {
        std::vector<int> points;
        int load = 0;
        for (size_t i = 0; i < r.size() - 1; ++i)
        {
            int node = r[i];
            if (graph.nodes[node].type == Node::PICKUP)
                load++;
            else if (graph.nodes[node].type == Node::DELIVERY)
                load--;

            if (load == 0 && i > 0)
                points.push_back(i);
        }
        return points;
    };

    std::vector<int> cuts1 = getEmptyPoints(r1, v1_id - 1);
    std::vector<int> cuts2 = getEmptyPoints(r2, v2_id - 1);

    if (cuts1.empty() || cuts2.empty())
        return false;

    int idx1 = cuts1[rand() % cuts1.size()];
    int idx2 = cuts2[rand() % cuts2.size()];

    std::vector<int> new_r1, new_r2;
    for (int k = 0; k <= idx1; ++k)
        new_r1.push_back(r1[k]);
    for (int k = idx2 + 1; k < (int)r2.size(); ++k)
        new_r1.push_back(r2[k]);

    for (int k = 0; k <= idx2; ++k)
        new_r2.push_back(r2[k]);
    for (int k = idx1 + 1; k < (int)r1.size(); ++k)
        new_r2.push_back(r1[k]);

    int idx1_v = v1_id - 1;
    int idx2_v = v2_id - 1;

    long long pen1_new, pen2_new;

    auto compatibleRoute = [&](const std::vector<int> &r, int veh_idx)
    {
        for (int node : r)
        {
            if (graph.nodes[node].type == Node::PICKUP)
            {
                int req = graph.nodes[node].request_id;
                if (!requests[req].isVehicleCompatible(vehicles[veh_idx].category))
                    return false;
            }
        }
        return true;
    };

    if (!compatibleRoute(new_r1, v1_id - 1))
        return false;
    if (!compatibleRoute(new_r2, v2_id - 1))
        return false;

    if (!checker.runEightStepEvaluation(new_r1, idx1_v))
        return false;
    pen1_new = checker.getPenalty();

    if (!checker.runEightStepEvaluation(new_r2, idx2_v))
        return false;
    pen2_new = checker.getPenalty();

    double old_cost = calculateRouteCost(r1, vehicles[v1_id - 1]) + calculateRouteCost(r2, vehicles[v2_id - 1]);
    long long old_pen = getRoutePenalty(r1, idx1_v) + getRoutePenalty(r2, idx2_v);
    double new_cost = calculateRouteCost(new_r1, vehicles[v1_id - 1]) + calculateRouteCost(new_r2, vehicles[v2_id - 1]);
    long long new_pen = pen1_new + pen2_new;

    if (new_cost + new_pen - old_cost + old_pen < threshold)
    {
        sol.routes[v1_id] = new_r1;
        sol.routes[v2_id] = new_r2;
        sol.total_cost += (new_cost - old_cost);
        return true;
    }
    return false;
}

bool Solver::operatorInsertUnserved(Solution &sol)
{
    // 1. Sanity Check: If everyone is served, do nothing.
    if (sol.unserved_requests.empty())
        return false;

    // 2. Pick a random unserved request
    int idx_in_list = rand() % sol.unserved_requests.size();
    int req_idx = sol.unserved_requests[idx_in_list];

    // 3. Attempt to insert it
    if (insertRequestBest(sol, req_idx, -1))
    {
        // 4. Success! Remove the request from the unserved list.
        sol.unserved_requests[idx_in_list] = sol.unserved_requests.back();
        sol.unserved_requests.pop_back();

        // 5. Update global cost
        sol.total_cost = calculateTotalCost(sol);

        return true;
    }

    // 6. Failure: No feasible spot found in the current configuration
    return false;
}

double Solver::calculateAverageEdgeCost()
{
    double sum = 0;
    int count = 0;
    for (int i = 0; i < 100; ++i)
    {
        int r1 = rand() % requests.size();
        // int r2 = rand() % requests.size();

        // sum += getDistance(requests[r1].pickup_loc, requests[r2].drop_loc);
        // request_id i → matrix index i; OFFICE → N+V  (same mapping as getMatrixIndex())
        sum += getDistanceByIndex(r1, N + V);
        count++;
    }
    return (count > 0) ? sum / count : 10.0;
}
// void Solver::buildInitialSolutionFromCSV(
//     Solution &sol,
//     const std::vector<InitialTrip> &trips)
// {
//     sol.routes.clear();
//     sol.unserved_requests.clear();

//     // 1. Initialize empty routes
//     for (const auto &veh : vehicles)
//     {
//         sol.routes[veh.id] = {
//             1 + (veh.id - 1),
//             graph.n + 1 + (veh.id - 1)};
//     }

//     // 2. Build lookup maps
//     std::unordered_map<std::string, int> vehMap;
//     for (const auto &v : vehicles)
//         vehMap[v.original_id] = v.id;

//     std::unordered_map<std::string, int> reqMap;
//     for (int i = 0; i < (int)requests.size(); i++)
//         reqMap[requests[i].original_id] = i;

//     // 3. Insert trips in given order
//     for (const auto &t : trips)
//     {
//         if (!vehMap.count(t.vehicle_id) || !reqMap.count(t.employee_id))
//             continue;

//         int veh_id = vehMap[t.vehicle_id];
//         int req_id = reqMap[t.employee_id];

//         int p = graph.getPickupNodeId(req_id);
//         int d = graph.getDeliveryNodeId(req_id);

//         auto &route = sol.routes[veh_id];

//         // Insert before END
//         route.insert(route.end() - 1, p);
//         route.insert(route.end() - 1, d);
//     }

//     // 4. Mark unserved requests
//     std::vector<bool> served(requests.size(), false);
//     for (auto &[vid, route] : sol.routes)
//         for (int n : route)
//             if (graph.nodes[n].type == Node::PICKUP)
//                 served[graph.nodes[n].request_id] = true;

//     for (int i = 0; i < (int)requests.size(); i++)
//         if (!served[i])
//             sol.unserved_requests.push_back(i);

//     // 5. Validate feasibility
//     for (auto &[vid, route] : sol.routes)
//     {
//         if (!checker.runEightStepEvaluation(route, vid - 1))
//         {
//             std::cerr << "Initial solution infeasible for vehicle " << vid << "\n";
//         }
//     }

//     // 6. Cost
//     sol.total_cost = calculateTotalCost(sol);
// }

// --- GRAPH MATCHING + CAPACITY AWARE INITIAL SOLUTION ---
// --- GRAPH MATCHING + CAPACITY + SHARING AWARE INITIAL SOLUTION ---
