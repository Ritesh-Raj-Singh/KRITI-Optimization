#include "evaluate.h"
#include "params.h"
#include <algorithm>
#include <limits>

double get_waiting_cost(double duration, char tariff_type, const DARPInstance &instance)
{
    if (duration <= 0.0 || tariff_type == '\0')
    {
        return 0.0;
    }

    const auto &tariff_table = (tariff_type == 'a') ? instance.tariff_a : instance.tariff_b;

    for (const auto &bracket : tariff_table)
    {
        if (duration <= bracket.first)
        {
            return bracket.second;
        }
    }

    return tariff_table.empty() ? 0.0 : tariff_table.back().second;
}

void calculate_route_stats(Route &route, DARPInstance &instance)
{
    if (route.stats_valid)
    {
        return;
    }

    route.f1 = 0.0;
    route.f2 = 0.0;
    route.viol_cap = 0.0;
    route.viol_tw = 0.0;
    route.viol_ride = 0.0;

    if (route.sequence.empty())
    {
        route.stats_valid = true;
        return;
    }

    const Vehicle &vehicle = *route.vehicle;
    double speed = vehicle.average_speed;
    double current_time = 0.0;

    std::vector<double> &pickup_times = instance.pickup_times_scratch;
    std::vector<int> &pickup_visited = instance.pickup_visited_scratch;
    pickup_visited.clear();

    double total_distance = 0.0;
    int load = 0;
    int sp_count[4] = {0};
    double viol_cap = 0.0, viol_tw = 0.0, viol_ride = 0.0;
    double cost_waiting = 0.0, employee_quality_sq = 0.0;

    int prev_node_id = vehicle.depot_id;
    const Node *prev_node = &instance.nodes[prev_node_id];

    const Node *first_node = &instance.nodes[route.sequence.front()];
    double dist_to_first = instance.get_dist(prev_node_id, first_node->id);
    double start_time = std::max(prev_node->e, first_node->e - (dist_to_first / speed) * 60.0); // Convert hours to minutes

    current_time = start_time;

    for (int node_id : route.sequence)
    {
        const Node &node = instance.nodes[node_id];

        double dist = instance.get_dist(prev_node_id, node.id);
        double travel_time = (dist / speed) * 60.0; // Convert hours to minutes

        total_distance += dist;
        double arrival_time = current_time + prev_node->st + travel_time;

        double start_service_time = std::max(node.e, arrival_time);

        double waiting_time = start_service_time - arrival_time;

        if (node.type == NodeType::DELIVERY || node.type == NodeType::PICKUP)
        {
            if (node.waiting_tariff != '\0')
            {
                cost_waiting += get_waiting_cost(waiting_time, node.waiting_tariff, instance);
            }
        }

        // Employee Quality Logic
        double wp_i = 0.0;
        if (node.type == NodeType::PICKUP && arrival_time > node.e)
            wp_i = arrival_time - node.e;
        else if (node.type == NodeType::DELIVERY && arrival_time < node.l)
            wp_i = node.l - arrival_time;
        employee_quality_sq += (wp_i * node.priority);

        // Load & TW Logic
        load += node.load;
        if (node.load > 0)
            sp_count[node.sharing_preference]++;
        else if (node.load < 0)
            sp_count[node.sharing_preference]--;
        int effective_cap = vehicle.capacity;
        for (int k = 1; k <= 3; ++k)
        {
            if (sp_count[k] > 0)
            {
                effective_cap = std::min(effective_cap, k);
                break;
            }
        }
        if (load > effective_cap)
            viol_cap += (load - effective_cap);
        if (start_service_time > node.l)
            viol_tw += (start_service_time - node.l) * node.priority;

        // Ride time logic
        if (node.type == NodeType::PICKUP)
        {
            pickup_times[node.id] = start_service_time;
            pickup_visited.push_back(node.id);
        }
        else if (node.type == NodeType::DELIVERY)
        {
            int req_id = instance.get_req_id_by_node(node.id);
            int pickup_node_id = instance.get_pickup_node_by_req(req_id);
            if (pickup_times[pickup_node_id] != -1.0)
            {
                double ride = start_service_time - (pickup_times[pickup_node_id] + instance.nodes[pickup_node_id].st);
                double max_r = instance.get_max_ride_time_by_req(req_id);
                if (ride > max_r)
                    viol_ride += (ride - max_r);
            }
        }
        current_time = start_service_time;
        prev_node = &node;
        prev_node_id = node.id;
    }

    // Cleanup scratchpad
    for (int id : pickup_visited)
        pickup_times[id] = -1.0;

    // Finalizing Logic for Return to Depot
    double dist_to_depot = instance.get_dist(prev_node_id, vehicle.depot_id);

    double cost_distance = total_distance;
    double cost_duration = (current_time + prev_node->st) - start_time;

    total_distance += dist_to_depot;

    // Cost Calculations
    double cost_dist = 0.0;
    if (cost_distance <= vehicle.L)
    {
        cost_dist = vehicle.lambda_k;
    }
    else
    {
        cost_dist = vehicle.lambda_k + vehicle.c_k * (cost_distance - vehicle.L);
    }

    double cost_time_total = vehicle.ct_k * cost_duration;

    int n_p = route.sequence.size() / 2;
    double cost_employee_total = 0.0;
    if (n_p > 1)
    {
        cost_employee_total = vehicle.cp * (n_p - 1);
    }

    route.f1 = cost_dist + cost_time_total + cost_employee_total + cost_waiting;
    route.f2 = employee_quality_sq;

    route.viol_cap = viol_cap;
    route.viol_tw = viol_tw;
    route.viol_ride = viol_ride;

    route.total_distance = cost_distance;
    route.total_duration = cost_duration;

    route.is_feasible = (viol_cap == 0.0 && viol_tw == 0.0 && viol_ride == 0.0);
    route.stats_valid = true;
}

double evaluate_solution(Solution &solution, DARPInstance &instance, double psi)
{
    double total_f1 = 0.0;
    double total_f2 = 0.0;

    double total_viol_cap = 0.0;
    double total_viol_tw = 0.0;
    double total_viol_ride = 0.0;

    for (auto &route : solution.routes)
    {
        calculate_route_stats(route, instance);
        total_f1 += route.f1;
        total_f2 += route.f2;
        total_viol_cap += route.viol_cap;
        total_viol_tw += route.viol_tw;
        total_viol_ride += route.viol_ride;
    }

    double weighted_f2 = psi * total_f2;

    double unassigned_penalty = Params::UNASSIGNED_PENALTY * solution.unassigned_requests.size();

    double f3 = (solution.alpha * total_viol_ride +
                 solution.beta * total_viol_tw +
                 solution.gamma * total_viol_cap +
                 unassigned_penalty);

    solution.f1 = total_f1;
    solution.f2 = weighted_f2;
    solution.f3 = f3;

    return solution.total_cost();
}

double update_solution_costs(Solution &solution, DARPInstance &instance, double psi)
{
    double total_f1 = 0.0;
    double total_f2 = 0.0;
    double total_viol_cap = 0.0;
    double total_viol_tw = 0.0;
    double total_viol_ride = 0.0;

    for (auto &route : solution.routes)
    {
        if (!route.stats_valid)
            calculate_route_stats(route, instance);
        total_f1 += route.f1;
        total_f2 += route.f2;
        total_viol_cap += route.viol_cap;
        total_viol_tw += route.viol_tw;
        total_viol_ride += route.viol_ride;
    }

    double weighted_f2 = psi * total_f2;
    double unassigned_penalty = Params::UNASSIGNED_PENALTY * solution.unassigned_requests.size();
    double f3 = (solution.alpha * total_viol_ride +
                 solution.beta * total_viol_tw +
                 solution.gamma * total_viol_cap +
                 unassigned_penalty);

    solution.f1 = total_f1;
    solution.f2 = weighted_f2;
    solution.f3 = f3;

    return solution.total_cost();
}

double calculate_route_cost_delta(Route &route, DARPInstance &instance, double alpha, double beta, double gamma)
{
    calculate_route_stats(route, instance);

    double f_val = (route.f1 +
                    Params::F2_WEIGHT * route.f2 +
                    alpha * route.viol_ride +
                    beta * route.viol_tw +
                    gamma * route.viol_cap);
    return f_val;
}

std::pair<double, std::vector<int>> insert_request_best_position(
    Route &route, const Request &request, DARPInstance &instance,
    double alpha, double beta, double gamma)
{
    double best_cost = std::numeric_limits<double>::infinity();
    int best_i = -1, best_j = -1;
    const int n = static_cast<int>(route.sequence.size());

    const Vehicle &vehicle = *route.vehicle;
    const double speed = vehicle.average_speed;
    const int depot_id = vehicle.depot_id;
    const Node &depot_node = instance.nodes[depot_id];

    const int pickup_id = request.pickup_node.id;
    const int delivery_id = request.delivery_node.id;
    const Node &pickup_node = instance.nodes[pickup_id];
    const Node &delivery_node = instance.nodes[delivery_id];

    const int n_p = (n + 2) / 2;
    const double cost_employee_total = (n_p > 1) ? vehicle.cp * (n_p - 1) : 0.0;

    struct EvalState
    {
        double cur_time;
        int load;
        int sp_count[4];
        double total_dist;
        double cost_waiting;
        double employee_q;
        double viol_cap;
        double viol_tw;
        double viol_ride;
        int prev_id;
        double prev_st;
    };

    // Static to avoid heap allocation/deallocation per call (single-threaded)
    static std::vector<EvalState> prefix;
    prefix.resize(n);
    double orig_start_time = 0.0;

    std::vector<double> &pickup_svc = instance.pickup_times_scratch;
    std::vector<int> &pickup_cleanup = instance.pickup_visited_scratch;
    pickup_cleanup.clear();

    if (n > 0)
    {
        const int first_id = route.sequence[0];
        const double dist_to_first = instance.get_dist(depot_id, first_id);
        orig_start_time = std::max(depot_node.e, instance.nodes[first_id].e - ((dist_to_first / speed) * 60.0));

        EvalState st{orig_start_time, 0, {0, 0, 0, 0}, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, depot_id, depot_node.st};

        for (int k = 0; k < n; ++k)
        {
            const int nid = route.sequence[k];
            const Node &nd = instance.nodes[nid];
            const double dist = instance.get_dist(st.prev_id, nid);
            st.total_dist += dist;
            const double arrival = st.cur_time + st.prev_st + (dist / speed) * 60.0; // Convert hours to minutes
            const double start_svc = std::max(nd.e, arrival);
            const double wait = start_svc - arrival;

            if ((nd.type == NodeType::DELIVERY || nd.type == NodeType::PICKUP) &&
                nd.waiting_tariff != '\0')
                st.cost_waiting += get_waiting_cost(wait, nd.waiting_tariff, instance);

            double wp = 0.0;
            if (nd.type == NodeType::PICKUP && arrival > nd.e)
                wp = arrival - nd.e;
            else if (nd.type == NodeType::DELIVERY && arrival < nd.l)
                wp = nd.l - arrival;
            st.employee_q += wp * nd.priority;

            st.load += nd.load;
            if (nd.load > 0)
                st.sp_count[nd.sharing_preference]++;
            else if (nd.load < 0)
                st.sp_count[nd.sharing_preference]--;
            {
                int eff_cap = vehicle.capacity;
                for (int sp = 1; sp <= 3; ++sp)
                {
                    if (st.sp_count[sp] > 0)
                    {
                        eff_cap = std::min(eff_cap, sp);
                        break;
                    }
                }
                if (st.load > eff_cap)
                    st.viol_cap += (st.load - eff_cap);
            }
            if (start_svc > nd.l)
                st.viol_tw += (start_svc - nd.l) * nd.priority;

            if (nd.type == NodeType::PICKUP)
            {
                pickup_svc[nid] = start_svc;
                pickup_cleanup.push_back(nid);
            }
            else if (nd.type == NodeType::DELIVERY)
            {
                const int rid = instance.get_req_id_by_node(nid);
                const int pid = instance.get_pickup_node_by_req(rid);
                if (pickup_svc[pid] != -1.0)
                {
                    const double ride = start_svc - (pickup_svc[pid] + instance.nodes[pid].st);
                    const double max_r = instance.get_max_ride_time_by_req(rid);
                    if (ride > max_r)
                        st.viol_ride += (ride - max_r);
                }
            }

            st.cur_time = start_svc;
            st.prev_id = nid;
            st.prev_st = nd.st;
            prefix[k] = st;
        }
    }

    auto process_node = [&](EvalState &s, int node_id, const Node &node) -> double
    {
        const double dist = instance.get_dist(s.prev_id, node_id);
        s.total_dist += dist;
        const double arrival = s.cur_time + s.prev_st + (dist / speed) * 60.0; // Convert hours to minutes
        const double start_svc = std::max(node.e, arrival);
        const double wait = start_svc - arrival;

        if ((node.type == NodeType::DELIVERY || node.type == NodeType::PICKUP) &&
            node.waiting_tariff != '\0')
            s.cost_waiting += get_waiting_cost(wait, node.waiting_tariff, instance);

        double wp = 0.0;
        if (node.type == NodeType::PICKUP && arrival > node.e)
            wp = arrival - node.e;
        else if (node.type == NodeType::DELIVERY && arrival < node.l)
            wp = node.l - arrival;
        s.employee_q += wp * node.priority;

        s.load += node.load;
        if (node.load > 0)
            s.sp_count[node.sharing_preference]++;
        else if (node.load < 0)
            s.sp_count[node.sharing_preference]--;
        {
            int eff_cap = vehicle.capacity;
            for (int k = 1; k <= 3; ++k)
            {
                if (s.sp_count[k] > 0)
                {
                    eff_cap = std::min(eff_cap, k);
                    break;
                }
            }
            if (s.load > eff_cap)
                s.viol_cap += (s.load - eff_cap);
        }
        if (start_svc > node.l)
            s.viol_tw += (start_svc - node.l) * node.priority;

        s.cur_time = start_svc;
        s.prev_id = node_id;
        s.prev_st = node.st;
        return start_svc;
    };

    static std::vector<std::pair<int, double>> mid_pickup_saves;
    static std::vector<std::pair<int, double>> suf_pickup_saves;
    mid_pickup_saves.clear();
    suf_pickup_saves.clear();

    for (int i = 0; i <= n; ++i)
    {
        EvalState base;
        double start_time;

        if (i == 0)
        {
            const double dist_to_pickup = instance.get_dist(depot_id, pickup_id);
            start_time = std::max(depot_node.e, pickup_node.e - (dist_to_pickup / speed) * 60.0); // Convert hours to minutes
            base = {start_time, 0, {0, 0, 0, 0}, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, depot_id, depot_node.st};
        }
        else
        {
            start_time = orig_start_time;
            base = prefix[i - 1];
        }

        EvalState after_pickup = base;
        const double pickup_start_svc = process_node(after_pickup, pickup_id, pickup_node);

        const double saved_new_pickup = pickup_svc[pickup_id];
        pickup_svc[pickup_id] = pickup_start_svc;

        mid_pickup_saves.clear();

        EvalState mid = after_pickup;

        for (int j = i + 1; j <= n + 1; ++j)
        {
            if (j > i + 1)
            {
                const int mid_id = route.sequence[j - 2];
                const Node &mid_node = instance.nodes[mid_id];
                const double mid_svc = process_node(mid, mid_id, mid_node);

                if (mid_node.type == NodeType::PICKUP)
                {
                    mid_pickup_saves.push_back({mid_id, pickup_svc[mid_id]});
                    pickup_svc[mid_id] = mid_svc;
                }
                else if (mid_node.type == NodeType::DELIVERY)
                {
                    const int rid = instance.get_req_id_by_node(mid_id);
                    const int pid = instance.get_pickup_node_by_req(rid);
                    if (pickup_svc[pid] != -1.0)
                    {
                        const double ride = mid_svc - (pickup_svc[pid] + instance.nodes[pid].st);
                        const double max_r = instance.get_max_ride_time_by_req(rid);
                        if (ride > max_r)
                            mid.viol_ride += (ride - max_r);
                    }
                }
            }

            EvalState after_del = mid;
            const double del_svc = process_node(after_del, delivery_id, delivery_node);

            {
                const double ride = del_svc - (pickup_start_svc + pickup_node.st);
                const double max_r = request.max_ride_time;
                if (ride > max_r)
                    after_del.viol_ride += (ride - max_r);
            }

            // Early termination: lower bound using only state accumulated so far
            // (suffix can only add more distance, time, violations, etc.)
            {
                double lb_dist = (after_del.total_dist <= vehicle.L)
                                     ? vehicle.lambda_k
                                     : (vehicle.lambda_k + vehicle.c_k * (after_del.total_dist - vehicle.L));
                double lb_f1 = lb_dist + vehicle.ct_k * ((after_del.cur_time + after_del.prev_st) - start_time)
                               + cost_employee_total + after_del.cost_waiting;
                double lb_fval = lb_f1 + Params::F2_WEIGHT * after_del.employee_q
                                 + alpha * after_del.viol_ride
                                 + beta * after_del.viol_tw
                                 + gamma * after_del.viol_cap;
                if (lb_fval >= best_cost)
                    continue;  // Skip O(n) suffix processing
            }

            EvalState suf = after_del;
            suf_pickup_saves.clear();

            for (int k = j - 1; k < n; ++k)
            {
                const int sid = route.sequence[k];
                const Node &snode = instance.nodes[sid];
                const double ssvc = process_node(suf, sid, snode);

                if (snode.type == NodeType::PICKUP)
                {
                    suf_pickup_saves.push_back({sid, pickup_svc[sid]});
                    pickup_svc[sid] = ssvc;
                }
                else if (snode.type == NodeType::DELIVERY)
                {
                    const int rid = instance.get_req_id_by_node(sid);
                    const int pid = instance.get_pickup_node_by_req(rid);
                    if (pickup_svc[pid] != -1.0)
                    {
                        const double ride = ssvc - (pickup_svc[pid] + instance.nodes[pid].st);
                        const double max_r = instance.get_max_ride_time_by_req(rid);
                        if (ride > max_r)
                            suf.viol_ride += (ride - max_r);
                    }
                }
            }

            for (auto it = suf_pickup_saves.rbegin(); it != suf_pickup_saves.rend(); ++it)
                pickup_svc[it->first] = it->second;

            const double cost_distance = suf.total_dist;
            const double cost_duration = (suf.cur_time + suf.prev_st) - start_time;
            const double cost_dist = (cost_distance <= vehicle.L)
                                         ? vehicle.lambda_k
                                         : (vehicle.lambda_k + vehicle.c_k * (cost_distance - vehicle.L));
            const double f1 = cost_dist + vehicle.ct_k * cost_duration +
                              cost_employee_total + suf.cost_waiting;
            const double f2 = suf.employee_q;
            const double f_val = f1 + Params::F2_WEIGHT * f2 +
                                 alpha * suf.viol_ride +
                                 beta * suf.viol_tw +
                                 gamma * suf.viol_cap;

            if (f_val < best_cost)
            {
                best_cost = f_val;
                best_i = i;
                best_j = j;
            }
        }

        for (auto it = mid_pickup_saves.rbegin(); it != mid_pickup_saves.rend(); ++it)
            pickup_svc[it->first] = it->second;

        pickup_svc[pickup_id] = saved_new_pickup;
    }

    for (int id : pickup_cleanup)
        pickup_svc[id] = -1.0;

    std::vector<int> best_seq;
    if (best_i != -1)
    {
        best_seq.reserve(n + 2);
        best_seq.insert(best_seq.end(), route.sequence.begin(),
                        route.sequence.begin() + best_i);
        best_seq.push_back(pickup_id);
        best_seq.insert(best_seq.end(), route.sequence.begin() + best_i,
                        route.sequence.begin() + best_j - 1);
        best_seq.push_back(delivery_id);
        best_seq.insert(best_seq.end(), route.sequence.begin() + best_j - 1,
                        route.sequence.end());
    }

    route.stats_valid = false;
    return std::make_pair(best_cost, best_seq);
}
