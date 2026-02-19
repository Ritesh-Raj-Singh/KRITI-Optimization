#include <iostream>
#include <iomanip>
#include "vns.h"
#include "init_sol.h"
#include "params.h"

#include <string>
#include <vector>
#include <random>
#include <algorithm>

static std::random_device rd;
static std::mt19937 gen(rd());

/**
 * Selects h requests randomly and moves them to other routes.
 * Works IN-PLACE on the solution.
 * Returns true if modifications were made.
 */
// Helper: save a route's sequence and stats to backups if not already saved
static void backup_route(RouteBackups &backups, int idx, const Route &route)
{
    for (auto &b : backups)
        if (b.index == idx) return;
    backups.push_back({idx, route.sequence, route.f1, route.f2,
                       route.viol_cap, route.viol_tw, route.viol_ride,
                       route.total_distance, route.total_duration, route.is_feasible});
}

bool neighborhood_move(Solution &solution, int h, DARPInstance &instance, RouteBackups &backups)
{
    std::vector<int> non_empty_indices;
    for (size_t i = 0; i < solution.routes.size(); ++i)
    {
        if (!solution.routes[i].sequence.empty())
        {
            non_empty_indices.push_back(i);
        }
    }
    if (non_empty_indices.empty())
    {
        return false;
    }

    bool modified = false;

    for (int k = 0; k < h; ++k)
    {
        if (non_empty_indices.empty())
            break;

        std::uniform_int_distribution<> dist_route(0, non_empty_indices.size() - 1);
        int source_idx = non_empty_indices[dist_route(gen)];
        Route &source_route = solution.routes[source_idx];

        std::vector<int> pickup_nodes;
        for (int node_id : source_route.sequence)
        {
            if (node_id < (int)instance.nodes.size() && instance.nodes[node_id].type == NodeType::PICKUP)
            {
                pickup_nodes.push_back(node_id);
            }
        }
        if (pickup_nodes.empty())
            continue;

        std::uniform_int_distribution<> dist_pickup(0, pickup_nodes.size() - 1);
        int p_node_id = pickup_nodes[dist_pickup(gen)];

        int req_id = instance.get_req_id_by_node(p_node_id);
        const Request &request = instance.get_request(req_id);
        int d_node_id = request.delivery_node.id;

        backup_route(backups, source_idx, source_route);

        auto &seq = source_route.sequence;
        seq.erase(std::remove(seq.begin(), seq.end(), p_node_id), seq.end());
        seq.erase(std::remove(seq.begin(), seq.end(), d_node_id), seq.end());

        source_route.invalidate_stats();

        std::vector<int> potential_indices;
        for (size_t i = 0; i < solution.routes.size(); ++i)
        {
            if (request.compatible_vehicle_types & (1u << solution.routes[i].vehicle->type_id))
            {
                potential_indices.push_back(i);
            }
        }
        if (potential_indices.empty())
            continue;

        std::uniform_int_distribution<> dist_dest(0, potential_indices.size() - 1);
        int dest_idx = potential_indices[dist_dest(gen)];
        Route &dest_route = solution.routes[dest_idx];

        backup_route(backups, dest_idx, dest_route);

        int n = dest_route.sequence.size();
        std::uniform_int_distribution<> dist_i(0, n);
        int i_pos = dist_i(gen);
        std::uniform_int_distribution<> dist_j(i_pos + 1, n + 1);
        int j_pos = dist_j(gen);

        auto it = dest_route.sequence.begin();
        dest_route.sequence.insert(it + i_pos, p_node_id);
        dest_route.sequence.insert(dest_route.sequence.begin() + j_pos, d_node_id);

        dest_route.invalidate_stats();

        modified = true;
    }
    return modified;
}

/**
 * Exchanges sequences between two routes.
 * Works IN-PLACE on the solution.
 */
bool neighborhood_swap(Solution &solution, int h, DARPInstance &instance, RouteBackups &backups)
{
    std::vector<int> route_indices;
    for (size_t i = 0; i < solution.routes.size(); ++i)
    {
        if (!solution.routes[i].sequence.empty())
        {
            route_indices.push_back(i);
        }
    }
    if (route_indices.size() < 2)
    {
        return false;
    }

    std::uniform_int_distribution<> dist_r(0, route_indices.size() - 1);
    int idx1 = dist_r(gen);
    int idx2;
    do
    {
        idx2 = dist_r(gen);
    } while (idx2 == idx1);

    Route &r1 = solution.routes[route_indices[idx1]];
    Route &r2 = solution.routes[route_indices[idx2]];

    int max_len1 = r1.sequence.size() / 2;
    int max_len2 = r2.sequence.size() / 2;
    if (max_len1 == 0 || max_len2 == 0)
    {
        return false;
    }

    std::uniform_int_distribution<> dist_len1(1, std::min(h, max_len1));
    std::uniform_int_distribution<> dist_len2(1, std::min(h, max_len2));
    int len1 = dist_len1(gen);
    int len2 = dist_len2(gen);

    auto extract_random_request_ids = [&instance](Route *route, int count) -> std::vector<int>
    {
        std::vector<int> req_ids;
        std::vector<int> p_indices;
        int idx = 0;
        for (auto it = route->sequence.begin(); it != route->sequence.end(); ++it, ++idx)
        {
            int node_id = *it;
            if (node_id < (int)instance.nodes.size() && instance.nodes[node_id].type == NodeType::PICKUP)
            {
                p_indices.push_back(idx);
            }
        }
        if (p_indices.empty())
            return req_ids;

        std::shuffle(p_indices.begin(), p_indices.end(), gen);
        if (count > (int)p_indices.size())
            count = p_indices.size();

        for (int k = 0; k < count; ++k)
        {
            int pos = p_indices[k];
            int p_id = route->sequence[pos];
            int r_id = instance.get_req_id_by_node(p_id);
            req_ids.push_back(r_id);
        }
        return req_ids;
    };

    std::vector<int> req_ids_1 = extract_random_request_ids(&r1, len1);
    std::vector<int> req_ids_2 = extract_random_request_ids(&r2, len2);

    // Compatibility checks using bitmask
    for (int req_id : req_ids_1)
    {
        const Request &req = instance.get_request(req_id);
        if (!(req.compatible_vehicle_types & (1u << r2.vehicle->type_id)))
            return false;
    }
    for (int req_id : req_ids_2)
    {
        const Request &req = instance.get_request(req_id);
        if (!(req.compatible_vehicle_types & (1u << r1.vehicle->type_id)))
            return false;
    }

    backup_route(backups, route_indices[idx1], r1);
    backup_route(backups, route_indices[idx2], r2);

    // Remove from both routes
    for (int req_id : req_ids_1)
    {
        const Request &req = instance.get_request(req_id);
        auto &seq = r1.sequence;
        seq.erase(std::remove(seq.begin(), seq.end(), req.pickup_node.id), seq.end());
        seq.erase(std::remove(seq.begin(), seq.end(), req.delivery_node.id), seq.end());
    }
    for (int req_id : req_ids_2)
    {
        const Request &req = instance.get_request(req_id);
        auto &seq = r2.sequence;
        seq.erase(std::remove(seq.begin(), seq.end(), req.pickup_node.id), seq.end());
        seq.erase(std::remove(seq.begin(), seq.end(), req.delivery_node.id), seq.end());
    }

    r1.invalidate_stats();
    r2.invalidate_stats();

    // Insert seq1 into r2
    for (int req_id : req_ids_1)
    {
        const Request &req = instance.get_request(req_id);
        int n = r2.sequence.size();
        std::uniform_int_distribution<> dist_i(0, n);
        int i_pos = dist_i(gen);
        std::uniform_int_distribution<> dist_j(i_pos + 1, n + 1);
        int j_pos = dist_j(gen);

        r2.sequence.insert(r2.sequence.begin() + i_pos, req.pickup_node.id);
        r2.sequence.insert(r2.sequence.begin() + j_pos, req.delivery_node.id);
    }

    // Insert seq2 into r1
    for (int req_id : req_ids_2)
    {
        const Request &req = instance.get_request(req_id);
        int n = r1.sequence.size();
        std::uniform_int_distribution<> dist_i(0, n);
        int i_pos = dist_i(gen);
        std::uniform_int_distribution<> dist_j(i_pos + 1, n + 1);
        int j_pos = dist_j(gen);

        r1.sequence.insert(r1.sequence.begin() + i_pos, req.pickup_node.id);
        r1.sequence.insert(r1.sequence.begin() + j_pos, req.delivery_node.id);
    }

    r1.invalidate_stats();
    r2.invalidate_stats();

    return true;
}

bool neighborhood_chain(Solution &solution, int h, DARPInstance &instance, RouteBackups &backups)
{
    std::vector<int> non_empty_indices;
    for (size_t i = 0; i < solution.routes.size(); ++i)
    {
        if (!solution.routes[i].sequence.empty())
        {
            non_empty_indices.push_back(i);
        }
    }
    if (non_empty_indices.empty())
        return false;

    std::uniform_int_distribution<> dist_route(0, non_empty_indices.size() - 1);
    int current_idx = non_empty_indices[dist_route(gen)];
    Route *current = &solution.routes[current_idx];

    bool modified = false;

    for (int step = 0; step < h; ++step)
    {
        if (current->sequence.empty())
            break;

        std::vector<int> pickups;
        for (int node_id : current->sequence)
        {
            if (node_id < (int)instance.nodes.size() && instance.nodes[node_id].type == NodeType::PICKUP)
            {
                pickups.push_back(node_id);
            }
        }
        if (pickups.empty())
            break;

        std::uniform_int_distribution<> dist_pick(0, pickups.size() - 1);
        int p_node_id = pickups[dist_pick(gen)];
        int req_id = instance.get_req_id_by_node(p_node_id);
        const Request &request = instance.get_request(req_id);
        int d_node_id = request.delivery_node.id;

        backup_route(backups, current_idx, *current);

        auto &seq = current->sequence;
        seq.erase(std::remove(seq.begin(), seq.end(), p_node_id), seq.end());
        seq.erase(std::remove(seq.begin(), seq.end(), d_node_id), seq.end());

        current->invalidate_stats();

        std::vector<int> potential_indices;
        for (size_t i = 0; i < solution.routes.size(); ++i)
        {
            if (&solution.routes[i] != current &&
                (request.compatible_vehicle_types & (1u << solution.routes[i].vehicle->type_id)))
            {
                potential_indices.push_back(i);
            }
        }
        if (potential_indices.empty())
            break;

        std::uniform_int_distribution<> dist_dest(0, potential_indices.size() - 1);
        int dest_idx = potential_indices[dist_dest(gen)];
        Route &dest = solution.routes[dest_idx];

        backup_route(backups, dest_idx, dest);

        int n = dest.sequence.size();
        std::uniform_int_distribution<> dist_i(0, n);
        int i_pos = dist_i(gen);
        std::uniform_int_distribution<> dist_j(i_pos + 1, n + 1);
        int j_pos = dist_j(gen);

        dest.sequence.insert(dest.sequence.begin() + i_pos, p_node_id);
        dest.sequence.insert(dest.sequence.begin() + j_pos, d_node_id);

        dest.invalidate_stats();

        current = &dest;
        current_idx = dest_idx;
        modified = true;
    }
    return modified;
}

/**
 * Single-pass local search (no convergence loop).
 * For each route, tries repositioning each request to its best position.
 * Uses in-place remove/restore to avoid vector copies.
 */
void apply_local_search(Solution &solution, DARPInstance &instance)
{
    for (auto &route : solution.routes)
    {
        // Skip routes that weren't modified by the neighborhood operation.
        // Between iterations, all routes have stats_valid = true.
        // Neighborhood ops invalidate only the routes they touch.
        if (route.sequence.empty() || route.stats_valid)
            continue;

        double best_route_cost = calculate_route_cost_delta(route, instance,
                                                            solution.alpha,
                                                            solution.beta,
                                                            solution.gamma);

        std::vector<int> pickup_nodes;
        for (int node_id : route.sequence)
        {
            if (instance.nodes[node_id].type == NodeType::PICKUP)
                pickup_nodes.push_back(node_id);
        }

        for (int p_node_id : pickup_nodes)
        {
            int req_id = instance.get_req_id_by_node(p_node_id);
            const Request &req = instance.get_request(req_id);
            int d_node_id = req.delivery_node.id;

            // Find positions of P and D in-place (avoids vector copies)
            auto &seq = route.sequence;
            int p_pos = -1, d_pos = -1;
            for (int idx = 0; idx < (int)seq.size(); ++idx)
            {
                if (seq[idx] == p_node_id)
                    p_pos = idx;
                else if (seq[idx] == d_node_id)
                    d_pos = idx;
            }
            if (p_pos == -1 || d_pos == -1)
                continue;

            // Remove larger index first to preserve smaller index
            int first_pos = std::min(p_pos, d_pos);
            int second_pos = std::max(p_pos, d_pos);
            int first_id = seq[first_pos];
            int second_id = seq[second_pos];

            seq.erase(seq.begin() + second_pos);
            seq.erase(seq.begin() + first_pos);
            route.invalidate_stats();

            auto result = insert_request_best_position(route, req, instance,
                                                       solution.alpha,
                                                       solution.beta,
                                                       solution.gamma);

            if (result.first < best_route_cost - Params::IMPROVEMENT_EPS)
            {
                route.sequence = std::move(result.second);
                route.invalidate_stats();
                best_route_cost = result.first;
            }
            else
            {
                // Restore in-place (insert smaller index first, then larger)
                seq.insert(seq.begin() + first_pos, first_id);
                seq.insert(seq.begin() + second_pos, second_id);
                route.invalidate_stats();
            }
        }
    }
    update_solution_costs(solution, instance, Params::LOCAL_SEARCH_PSI);
}

Solution vns1(
    const Solution &initial_solution,
    DARPInstance &instance,
    long long int k_max,
    int h)
{
    Solution current = initial_solution;

    apply_local_search(current, instance);

    // Try to reinsert any initially unassigned requests
    if (!current.unassigned_requests.empty())
    {
        std::vector<int> still_unassigned;
        for (int req_id : current.unassigned_requests)
        {
            const Request &req = instance.get_request(req_id);
            double best_insert_cost = std::numeric_limits<double>::infinity();
            Route *best_route = nullptr;
            std::vector<int> best_seq;

            for (auto &route : current.routes)
            {
                if (!(req.compatible_vehicle_types & (1u << route.vehicle->type_id)))
                    continue;
                auto result = insert_request_best_position(
                    route, req, instance,
                    current.alpha, current.beta, current.gamma);
                if (result.first < best_insert_cost)
                {
                    best_insert_cost = result.first;
                    best_route = &route;
                    best_seq = std::move(result.second);
                }
            }
            if (best_route && best_insert_cost < std::numeric_limits<double>::infinity())
            {
                best_route->sequence = std::move(best_seq);
                best_route->invalidate_stats();
            }
            else
            {
                still_unassigned.push_back(req_id);
            }
        }
        current.unassigned_requests = std::move(still_unassigned);
    }

    double best_cost = evaluate_solution(current, instance);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Best Cost " << current.f1 + current.f2 + current.f3
              << " (Feasible: " << (current.f3 == 0 ? "true" : "false") << ")\n";

    long long int current_iteration = 1;
    int neighbourhood_index = 1;
    const int NUM_NEIGHBOURHOODS = 3;

    while (current_iteration <= k_max)
    {
        RouteBackups backups;
        std::vector<int> unassigned_backup = current.unassigned_requests;
        // Save solution-level costs for fast restore on reject
        double saved_f1 = current.f1, saved_f2 = current.f2, saved_f3 = current.f3;

        bool modified = false;
        if (neighbourhood_index == 1)
        {
            modified = neighborhood_move(current, h, instance, backups);
        }
        else if (neighbourhood_index == 2)
        {
            modified = neighborhood_swap(current, h, instance, backups);
        }
        else if (neighbourhood_index == 3)
        {
            modified = neighborhood_chain(current, h, instance, backups);
        }

        if (!modified)
        {
            // Restore any partially modified routes to their original state and stats
            for (auto &b : backups)
            {
                Route &r = current.routes[b.index];
                r.sequence = std::move(b.sequence);
                r.f1 = b.f1; r.f2 = b.f2;
                r.viol_cap = b.viol_cap; r.viol_tw = b.viol_tw; r.viol_ride = b.viol_ride;
                r.total_distance = b.total_distance; r.total_duration = b.total_duration;
                r.is_feasible = b.is_feasible;
                r.stats_valid = true;
            }
            // No need to re-evaluate — solution-level f1/f2/f3 were never changed
            neighbourhood_index = (neighbourhood_index % NUM_NEIGHBOURHOODS) + 1;
            current_iteration++;
            continue;
        }

        apply_local_search(current, instance);

        // Try to reinsert unassigned requests
        if (!current.unassigned_requests.empty())
        {
            std::vector<int> still_unassigned;
            bool reinserted = false;
            for (int req_id : current.unassigned_requests)
            {
                const Request &req = instance.get_request(req_id);
                double best_insert_cost = std::numeric_limits<double>::infinity();
                Route *best_route = nullptr;
                std::vector<int> best_seq;

                for (auto &route : current.routes)
                {
                    if (!(req.compatible_vehicle_types & (1u << route.vehicle->type_id)))
                        continue;
                    auto result = insert_request_best_position(
                        route, req, instance,
                        current.alpha, current.beta, current.gamma);
                    if (result.first < best_insert_cost)
                    {
                        best_insert_cost = result.first;
                        best_route = &route;
                        best_seq = std::move(result.second);
                    }
                }
                if (best_route && best_insert_cost < std::numeric_limits<double>::infinity())
                {
                    best_route->sequence = std::move(best_seq);
                    best_route->invalidate_stats();
                    reinserted = true;
                }
                else
                {
                    still_unassigned.push_back(req_id);
                }
            }
            current.unassigned_requests = std::move(still_unassigned);
            if (reinserted)
                update_solution_costs(current, instance);
        }

        double candidate_cost = current.total_cost();

        if (candidate_cost < best_cost)
        {
            // Accept: discard backups, keep current as-is
            best_cost = candidate_cost;
            neighbourhood_index = 1;

            std::cout << "Best Cost " << current.f1 + current.f2 + current.f3
                      << " (Feasible: " << (current.f3 == 0 ? "true" : "false") << ")\n";
        }
        else
        {
            // Reject: restore modified routes from backups with saved stats
            for (auto &b : backups)
            {
                Route &r = current.routes[b.index];
                r.sequence = std::move(b.sequence);
                r.f1 = b.f1; r.f2 = b.f2;
                r.viol_cap = b.viol_cap; r.viol_tw = b.viol_tw; r.viol_ride = b.viol_ride;
                r.total_distance = b.total_distance; r.total_duration = b.total_duration;
                r.is_feasible = b.is_feasible;
                r.stats_valid = true;
            }
            current.unassigned_requests = std::move(unassigned_backup);
            // Restore solution-level costs without recalculation
            current.f1 = saved_f1; current.f2 = saved_f2; current.f3 = saved_f3;
            neighbourhood_index = (neighbourhood_index % NUM_NEIGHBOURHOODS) + 1;
        }

        current_iteration++;
    }

    return current;
}
