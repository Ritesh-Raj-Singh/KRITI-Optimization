#include "evaluate.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <set>

#include "params.h"

double get_waiting_cost(double duration, char tariff_type, const DARPInstance &instance) {
    if (duration <= 0.0 || tariff_type == '\0') {
        return 0.0;
    }

    const auto &tariff_table = (tariff_type == 'a') ? instance.tariff_a : instance.tariff_b;

    for (const auto &bracket : tariff_table) {
        if (duration <= bracket.first) {
            return bracket.second;
        }
    }

    return tariff_table.empty() ? 0.0 : tariff_table.back().second;
}

void calculate_route_stats(Route &route, DARPInstance &instance) {
    if (route.stats_valid) {
        return;
    }

    route.f1 = 0.0;
    route.f2 = 0.0;
    route.viol_cap = 0.0;
    route.viol_tw = 0.0;
    route.viol_ride = 0.0;

    if (route.sequence.empty()) {
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
    double start_time = std::max(prev_node->e, first_node->e - (dist_to_first / speed) * 60.0);

    current_time = start_time;

    for (int node_id : route.sequence) {
        const Node &node = instance.nodes[node_id];

        double dist = instance.get_dist(prev_node_id, node.id);
        double travel_time = (dist / speed) * 60.0;

        total_distance += dist;
        double arrival_time = current_time + prev_node->st + travel_time;

        double start_service_time = std::max(node.e, arrival_time);

        double waiting_time = start_service_time - arrival_time;

        if (node.type == NodeType::DELIVERY || node.type == NodeType::PICKUP) {
            if (node.waiting_tariff != '\0') {
                cost_waiting += get_waiting_cost(waiting_time, node.waiting_tariff, instance);
            }
        }

        double wp_i = 0.0;
        if (node.type == NodeType::PICKUP && arrival_time > node.e)
            wp_i = arrival_time - node.e;
        else if (node.type == NodeType::DELIVERY && arrival_time < node.l)
            wp_i = node.l - arrival_time;
        employee_quality_sq += (wp_i * node.priority);

        load += node.load;
        if (node.load > 0)
            sp_count[node.sharing_preference]++;
        else if (node.load < 0)
            sp_count[node.sharing_preference]--;
        int effective_cap = vehicle.capacity;
        for (int k = 1; k <= 3; ++k) {
            if (sp_count[k] > 0) {
                effective_cap = std::min(effective_cap, k);
                break;
            }
        }
        if (load > effective_cap) viol_cap += (load - effective_cap);
        if (start_service_time > node.l) viol_tw += (start_service_time - node.l) * node.priority;

        if (node.type == NodeType::PICKUP) {
            pickup_times[node.id] = start_service_time;
            pickup_visited.push_back(node.id);
        } else if (node.type == NodeType::DELIVERY) {
            int req_id = instance.get_req_id_by_node(node.id);
            int pickup_node_id = instance.get_pickup_node_by_req(req_id);
            if (pickup_times[pickup_node_id] != -1.0) {
                double ride = start_service_time - (pickup_times[pickup_node_id] + instance.nodes[pickup_node_id].st);
                double max_r = instance.get_max_ride_time_by_req(req_id);
                if (ride > max_r) viol_ride += (ride - max_r);
            }
        }
        current_time = start_service_time;
        prev_node = &node;
        prev_node_id = node.id;
    }

    for (int id : pickup_visited) pickup_times[id] = -1.0;

    double dist_to_depot = instance.get_dist(prev_node_id, vehicle.depot_id);

    double cost_distance = total_distance;
    double cost_duration = (current_time + prev_node->st) - start_time;

    total_distance += dist_to_depot;

    double cost_dist = 0.0;
    if (cost_distance <= vehicle.L) {
        cost_dist = vehicle.lambda_k;
    } else {
        cost_dist = vehicle.lambda_k + vehicle.c_k * (cost_distance - vehicle.L);
    }

    double cost_time_total = vehicle.ct_k * cost_duration;

    int n_p = route.sequence.size() / 2;
    double cost_employee_total = 0.0;
    if (n_p > 1) {
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

double evaluate_solution(Solution &solution, DARPInstance &instance, double psi) {
    double total_f1 = 0.0;
    double total_f2 = 0.0;

    double total_viol_cap = 0.0;
    double total_viol_tw = 0.0;
    double total_viol_ride = 0.0;

    for (auto &route : solution.routes) {
        calculate_route_stats(route, instance);
        total_f1 += route.f1;
        total_f2 += route.f2;
        total_viol_cap += route.viol_cap;
        total_viol_tw += route.viol_tw;
        total_viol_ride += route.viol_ride;
    }

    double weighted_f2 = psi * total_f2;

    double unassigned_penalty = Params::UNASSIGNED_PENALTY * solution.unassigned_requests.size();

    double f3 = (solution.alpha * total_viol_ride + solution.beta * total_viol_tw + solution.gamma * total_viol_cap +
                 unassigned_penalty);

    solution.f1 = total_f1;
    solution.f2 = weighted_f2;
    solution.f3 = f3;

    return solution.total_cost();
}

double update_solution_costs(Solution &solution, DARPInstance &instance, double psi) {
    double total_f1 = 0.0;
    double total_f2 = 0.0;
    double total_viol_cap = 0.0;
    double total_viol_tw = 0.0;
    double total_viol_ride = 0.0;

    for (auto &route : solution.routes) {
        if (!route.stats_valid) calculate_route_stats(route, instance);
        total_f1 += route.f1;
        total_f2 += route.f2;
        total_viol_cap += route.viol_cap;
        total_viol_tw += route.viol_tw;
        total_viol_ride += route.viol_ride;
    }

    double weighted_f2 = psi * total_f2;
    double unassigned_penalty = Params::UNASSIGNED_PENALTY * solution.unassigned_requests.size();
    double f3 = (solution.alpha * total_viol_ride + solution.beta * total_viol_tw + solution.gamma * total_viol_cap +
                 unassigned_penalty);

    solution.f1 = total_f1;
    solution.f2 = weighted_f2;
    solution.f3 = f3;

    return solution.total_cost();
}

double calculate_route_cost_delta(Route &route, DARPInstance &instance, double alpha, double beta, double gamma) {
    calculate_route_stats(route, instance);

    double f_val = (route.f1 + Params::F2_WEIGHT * route.f2 + alpha * route.viol_ride + beta * route.viol_tw +
                    gamma * route.viol_cap);
    return f_val;
}

std::pair<double, std::vector<int>> insert_request_best_position(Route &route, const Request &request,
                                                                 DARPInstance &instance, double alpha, double beta,
                                                                 double gamma) {
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

    struct EvalState {
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

    auto process_node = [&](EvalState &s, int node_id, const Node &node) -> double {
        const double dist = instance.get_dist(s.prev_id, node_id);
        s.total_dist += dist;
        const double arrival = s.cur_time + s.prev_st + (dist / speed) * 60.0;
        const double start_svc = std::max(node.e, arrival);
        const double wait = start_svc - arrival;

        if ((node.type == NodeType::DELIVERY || node.type == NodeType::PICKUP) && node.waiting_tariff != '\0')
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

        int eff_cap = vehicle.capacity;
        for (int k = 1; k <= 3; ++k) {
            if (s.sp_count[k] > 0) {
                eff_cap = std::min(eff_cap, k);
                break;
            }
        }
        if (s.load > eff_cap) s.viol_cap += (s.load - eff_cap);

        if (start_svc > node.l) s.viol_tw += (start_svc - node.l) * node.priority;

        s.cur_time = start_svc;
        s.prev_id = node_id;
        s.prev_st = node.st;
        return start_svc;
    };

    std::vector<double> &pickup_svc = instance.pickup_times_scratch;
    std::vector<int> &pickup_cleanup = instance.pickup_visited_scratch;
    pickup_cleanup.clear();

    static std::vector<EvalState> base_prefix;
    if ((int)base_prefix.size() < n + 1) base_prefix.resize(n + 1);

    double orig_start_time = 0.0;
    if (n > 0) {
        const int first = route.sequence[0];
        const double d = instance.get_dist(depot_id, first);
        orig_start_time = std::max(depot_node.e, instance.nodes[first].e - (d / speed) * 60.0);

        EvalState st{orig_start_time, 0, {0, 0, 0, 0}, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, depot_id, depot_node.st};
        for (int k = 0; k < n; ++k) {
            const int nid = route.sequence[k];
            const Node &nd = instance.nodes[nid];
            double svc = process_node(st, nid, nd);
            if (nd.type == NodeType::PICKUP) {
                pickup_svc[nid] = svc;
                pickup_cleanup.push_back(nid);
            }
            base_prefix[k] = st;
        }
    }

    static std::vector<std::pair<int, double>> pickup_restores;

    for (int i = 0; i <= n; ++i) {
        static std::vector<EvalState> ref_traj;
        static std::vector<double> ref_cap_adj;
        if ((int)ref_traj.size() < n + 2) ref_traj.resize(n + 2);
        if ((int)ref_cap_adj.size() < n + 2) ref_cap_adj.resize(n + 2);

        EvalState st;
        if (i == 0) {
            const double d_to_p = instance.get_dist(depot_id, pickup_id);
            double st_time = std::max(depot_node.e, pickup_node.e - (d_to_p / speed) * 60.0);
            st = {st_time, 0, {0, 0, 0, 0}, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, depot_id, depot_node.st};
        } else {
            st = base_prefix[i - 1];
        }

        double p_start_svc = process_node(st, pickup_id, pickup_node);

        double saved_p_svc = pickup_svc[pickup_id];
        pickup_svc[pickup_id] = p_start_svc;

        ref_traj[i] = st;

        bool possible = true;
        for (int k = i; k < n; ++k) {
            int nid = route.sequence[k];
            const Node &nd = instance.nodes[nid];
            double svc = process_node(st, nid, nd);

            pickup_restores.push_back({nid, pickup_svc[nid]});
            if (nd.type == NodeType::PICKUP) {
                pickup_svc[nid] = svc;
            } else if (nd.type == NodeType::DELIVERY) {
                int rid = instance.get_req_id_by_node(nid);
                int pid = instance.get_pickup_node_by_req(rid);
                if (pickup_svc[pid] != -1.0) {
                    double ride = svc - (pickup_svc[pid] + instance.nodes[pid].st);
                    double max_r = instance.get_max_ride_time_by_req(rid);
                    if (ride > max_r) st.viol_ride += (ride - max_r);
                }
            }
            ref_traj[k + 1] = st;
        }

        double acc_cap_adj = 0.0;
        int p_pref = request.pickup_node.sharing_preference;

        for (int k = n; k > i; --k) {
            EvalState &s = ref_traj[k];

            int mod_load = s.load - 1;
            int mod_sp[4] = {s.sp_count[0], s.sp_count[1], s.sp_count[2], s.sp_count[3]};
            mod_sp[p_pref]--;

            double mod_viol = 0.0;
            int eff_cap = vehicle.capacity;
            for (int m = 1; m <= 3; ++m) {
                if (mod_sp[m] > 0) {
                    eff_cap = std::min(eff_cap, m);
                    break;
                }
            }
            if (mod_load > eff_cap) mod_viol = (mod_load - eff_cap);

            double delta = mod_viol - s.viol_cap;

            double prev_viol = ref_traj[k - 1].viol_cap;
            double local_viol = s.viol_cap - prev_viol;

            acc_cap_adj += (mod_viol - local_viol);
            ref_cap_adj[k] = acc_cap_adj;
        }

        for (int j = i + 1; j <= n + 1; ++j) {
            EvalState st_j = ref_traj[j - 1];

            double d_start_svc = process_node(st_j, delivery_id, delivery_node);

            {
                double ride = d_start_svc - (p_start_svc + pickup_node.st);
                if (ride > request.max_ride_time) st_j.viol_ride += (ride - request.max_ride_time);
            }

            if (j == n + 1) {
                double dist_to_depot = instance.get_dist(st_j.prev_id, depot_id);
                double total_dist = st_j.total_dist + dist_to_depot;

                double cost_dist = (total_dist <= vehicle.L)
                                       ? vehicle.lambda_k
                                       : (vehicle.lambda_k + vehicle.c_k * (total_dist - vehicle.L));
                double cost_dur = vehicle.ct_k * ((st_j.cur_time + st_j.prev_st) - orig_start_time);

                double f1 = cost_dist + cost_dur + cost_employee_total + st_j.cost_waiting;
                double f2 = st_j.employee_q;
                double cost =
                    f1 + Params::F2_WEIGHT * f2 + alpha * st_j.viol_ride + beta * st_j.viol_tw + gamma * st_j.viol_cap;

                if (cost < best_cost) {
                    best_cost = cost;
                    best_i = i;
                    best_j = j;
                }
            } else {
                int next_nid = route.sequence[j - 1];
                double dist_d_next = instance.get_dist(delivery_id, next_nid);
                double arrival_at_next = st_j.cur_time + delivery_node.st + (dist_d_next / speed) * 60.0;

                EvalState st_next = st_j;
                double next_start_svc = process_node(st_next, next_nid, instance.nodes[next_nid]);

                if (std::abs(next_start_svc - ref_traj[j].cur_time) < 1e-4) {
                    EvalState &ref_end = ref_traj[n];
                    EvalState &ref_curr = ref_traj[j];

                    st_next.total_dist += (ref_end.total_dist - ref_curr.total_dist);
                    st_next.cost_waiting += (ref_end.cost_waiting - ref_curr.cost_waiting);
                    st_next.employee_q += (ref_end.employee_q - ref_curr.employee_q);
                    st_next.viol_tw += (ref_end.viol_tw - ref_curr.viol_tw);
                    st_next.viol_ride += (ref_end.viol_ride - ref_curr.viol_ride);

                    double cap_base_diff = (ref_end.viol_cap - ref_curr.viol_cap);
                    double cap_adj = ref_cap_adj[j + 1];

                    st_next.viol_cap += (cap_base_diff + cap_adj);

                    st_next.prev_id = ref_end.prev_id;
                    st_next.prev_st = ref_end.prev_st;
                    st_next.cur_time = ref_end.cur_time;

                    double dist_to_depot = instance.get_dist(st_next.prev_id, depot_id);
                    double total_dist = st_next.total_dist + dist_to_depot;
                    double cost_dist = (total_dist <= vehicle.L)
                                           ? vehicle.lambda_k
                                           : (vehicle.lambda_k + vehicle.c_k * (total_dist - vehicle.L));
                    double cost_dur = vehicle.ct_k * ((st_next.cur_time + st_next.prev_st) - orig_start_time);

                    double f1 = cost_dist + cost_dur + cost_employee_total + st_next.cost_waiting;
                    double f2 = st_next.employee_q;
                    double cost = f1 + Params::F2_WEIGHT * f2 + alpha * st_next.viol_ride + beta * st_next.viol_tw +
                                  gamma * st_next.viol_cap;

                    if (cost < best_cost) {
                        best_cost = cost;
                        best_i = i;
                        best_j = j;
                    }
                } else {
                    bool aborted = false;
                    for (int k = j + 1; k <= n; ++k) {
                        int nid = route.sequence[k - 1];
                        const Node &nd = instance.nodes[nid];
                        double svc = process_node(st_next, nid, nd);

                        if (nd.type == NodeType::DELIVERY) {
                            int rid = instance.get_req_id_by_node(nid);
                            int pid = instance.get_pickup_node_by_req(rid);

                            if (pickup_svc[pid] != -1.0) {
                                double ride = svc - (pickup_svc[pid] + instance.nodes[pid].st);
                                double max_r = instance.get_max_ride_time_by_req(rid);
                                if (ride > max_r) st_next.viol_ride += (ride - max_r);
                            }
                        } else if (nd.type == NodeType::PICKUP) {
                            pickup_restores.push_back({nid, pickup_svc[nid]});
                            pickup_svc[nid] = svc;
                        }
                    }

                    double dist_to_depot = instance.get_dist(st_next.prev_id, depot_id);
                    double total_dist = st_next.total_dist + dist_to_depot;
                    double cost_dist = (total_dist <= vehicle.L)
                                           ? vehicle.lambda_k
                                           : (vehicle.lambda_k + vehicle.c_k * (total_dist - vehicle.L));
                    double cost_dur = vehicle.ct_k * ((st_next.cur_time + st_next.prev_st) - orig_start_time);

                    double f1 = cost_dist + cost_dur + cost_employee_total + st_next.cost_waiting;
                    double f2 = st_next.employee_q;
                    double cost = f1 + Params::F2_WEIGHT * f2 + alpha * st_next.viol_ride + beta * st_next.viol_tw +
                                  gamma * st_next.viol_cap;

                    if (cost < best_cost) {
                        best_cost = cost;
                        best_i = i;
                        best_j = j;
                    }
                }
            }
        }

        for (auto it = pickup_restores.rbegin(); it != pickup_restores.rend(); ++it) {
            pickup_svc[it->first] = it->second;
        }
        pickup_restores.clear();
        pickup_svc[pickup_id] = saved_p_svc;
    }

    for (int id : pickup_cleanup) pickup_svc[id] = -1.0;

    std::vector<int> best_seq;
    if (best_i != -1) {
        best_seq.reserve(n + 2);
        best_seq.insert(best_seq.end(), route.sequence.begin(), route.sequence.begin() + best_i);
        best_seq.push_back(pickup_id);
        best_seq.insert(best_seq.end(), route.sequence.begin() + best_i, route.sequence.begin() + best_j - 1);
        best_seq.push_back(delivery_id);
        best_seq.insert(best_seq.end(), route.sequence.begin() + best_j - 1, route.sequence.end());
    }

    route.stats_valid = false;
    return std::make_pair(best_cost, best_seq);
}

void validate_solution_integrity(const Solution &solution, const DARPInstance &instance) {
    std::set<int> found_requests;
    for (const auto &route : solution.routes)
        for (int node_id : route.sequence)
            if (node_id < (int)instance.nodes.size() && instance.nodes[node_id].type == NodeType::PICKUP)
                found_requests.insert(instance.get_req_id_by_node(node_id));

    for (int req_id : solution.unassigned_requests) found_requests.insert(req_id);

    if ((int)found_requests.size() != (int)instance.requests.size()) {
        std::cerr << "INTEGRITY ERROR: " << found_requests.size() << " requests accounted for, expected "
                  << instance.requests.size() << "\n";
        for (const auto &req : instance.requests) {
            if (found_requests.find(req.id) == found_requests.end())
                std::cerr << "  Missing request ID: " << req.id << "\n";
        }
    }
}
