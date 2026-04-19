#define _USE_MATH_DEFINES
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "globals.h"
#include "matrix.h"

namespace fs = std::filesystem;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 1. DATA STRUCTURES & FAST MATRIX

const double INF = std::numeric_limits<double>::max();

// O(1) Lookup Table for Distances
std::vector<std::vector<double>> fast_distance;
int office_matrix_id = -1;

// Helper to convert minutes to HH:MM
std::string minToTime(int m) {
    int hrs = (m / 60) % 24;
    int mins = m % 60;
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hrs << ":" << std::setw(2) << mins;
    return ss.str();
}

// Helper to convert HH:MM string to minutes
int timeStringToMin(const std::string &t_str) {
    if (t_str.empty()) return 0;
    int hrs = 0, mins = 0;
    char colon;
    std::stringstream ss(t_str);
    ss >> hrs >> colon >> mins;
    return hrs * 60 + mins;
}

// Helper to capitalize category
std::string capitalize(std::string s) {
    if (s.empty()) return s;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    s[0] = std::toupper(s[0]);
    return s;
}

int allowedLoad(const std::string &pref) {
    if (pref == "single") return 1;
    if (pref == "double") return 2;
    if (pref == "triple") return 3;
    return 1000;
}

struct Config {
    double cost_weight = 1.0;
    double time_weight = 0.0;

    double alpha = 5000.0;
    ;                         // Ride Time Penalty
    double beta = 10000.0;    // Time Window Penalty
    double gamma = 100000.0;  // Capacity/Sharing Penalty

    std::vector<int> max_delays = {0, 10, 20, 30, 45, 60};
};

enum NodeType { PICKUP, DROP };

struct Request {
    int id;
    std::string original_id;
    int priority;
    double pickup_lat, pickup_lng;
    double drop_lat, drop_lng;
    int earliest_pickup;
    int latest_drop;
    std::string vehicle_pref;
    std::string sharing_pref;

    bool pref_premium = false;
    int max_sharing = 1000;

    int matrix_id = -1;
};

struct Vehicle {
    int id;
    std::string original_id;
    int capacity;
    double cost_per_km;
    double avg_speed_kmph;
    double current_lat, current_lng;
    int available_from;
    std::string category;

    bool is_premium = false;

    int matrix_id = -1;
};

struct Node {
    NodeType type;
    int emp_index;
    double arrival_time;
    double departure_time;
};

struct RouteMetrics {
    bool feasible;
    double total_cost;
    double total_time;
    double total_dist;
    double ride_time_violation;
    double time_window_violation;
    double capacity_violation;
    double objective_score;
};

// 2. FILE LOADING & INITIALIZATION

void loadMetadata(const std::string &filename, Config &config) {
    std::ifstream file(filename);
    if (!file.is_open()) return;
    std::string line;
    std::getline(file, line);

    if (config.max_delays.size() < 6) config.max_delays.resize(6, 999);

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string key, valStr;
        std::getline(ss, key, ',');
        std::getline(ss, valStr, ',');
        key.erase(std::remove(key.begin(), key.end(), '\r'), key.end());
        valStr.erase(std::remove(valStr.begin(), valStr.end(), '\r'), valStr.end());

        if (key == "priority_1_max_delay_min")
            config.max_delays[1] = std::stoi(valStr);
        else if (key == "priority_2_max_delay_min")
            config.max_delays[2] = std::stoi(valStr);
        else if (key == "priority_3_max_delay_min")
            config.max_delays[3] = std::stoi(valStr);
        else if (key == "priority_4_max_delay_min")
            config.max_delays[4] = std::stoi(valStr);
        else if (key == "priority_5_max_delay_min")
            config.max_delays[5] = std::stoi(valStr);
        else if (key == "objective_cost_weight")
            config.cost_weight = std::stod(valStr);
        else if (key == "objective_time_weight")
            config.time_weight = std::stod(valStr);
    }
}

std::vector<Vehicle> loadVehicles(const std::string &filename) {
    std::vector<Vehicle> vehicles;
    std::ifstream file(filename);
    if (!file.is_open()) exit(1);
    std::string line;
    std::getline(file, line);
    int sequential_id = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> row;
        while (std::getline(ss, token, ',')) {
            token.erase(0, token.find_first_not_of(" \t\r\n\""));
            token.erase(token.find_last_not_of(" \t\r\n\"") + 1);
            row.push_back(token);
        }
        if (row.size() < 8) continue;
        Vehicle v;
        v.id = sequential_id++;
        v.original_id = row[0];
        std::string catStr = row[9];

        if (catStr == "premium" || catStr == "Premium") {
            v.category = "premium";
            v.is_premium = true;
        } else if (catStr == "normal" || catStr == "Normal") {
            v.category = "normal";
        } else
            v.category = "any";

        try {
            v.capacity = std::stoi(row[3]);
            v.cost_per_km = std::stod(row[4]);
            v.avg_speed_kmph = std::stod(row[5]);
            if (v.avg_speed_kmph <= 0.0) {
                continue;
            }
            v.current_lat = std::stod(row[6]);
            v.current_lng = std::stod(row[7]);
            v.available_from = timeStringToMin(row[8]);
        } catch (...) {
            continue;
        }
        vehicles.push_back(v);
    }
    std::cout << "Loaded " << vehicles.size() << " vehicles from " << filename << "\n";
    return vehicles;
}

std::vector<Request> loadRequests(const std::string &filename, const std::vector<int> &priority_delays) {
    std::vector<Request> requests;
    std::ifstream file(filename);
    if (!file.is_open()) exit(1);
    std::string line;
    std::getline(file, line);
    int sequential_id = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> row;
        while (std::getline(ss, token, ',')) {
            token.erase(0, token.find_first_not_of(" \t\r\n\""));
            token.erase(token.find_last_not_of(" \t\r\n\"") + 1);
            row.push_back(token);
        }
        if (row.size() < 10) continue;
        Request r;
        r.id = sequential_id++;
        r.original_id = row[0];
        try {
            r.priority = std::stoi(row[1]);
            r.pickup_lat = std::stod(row[2]);
            r.pickup_lng = std::stod(row[3]);
            r.drop_lat = std::stod(row[4]);
            r.drop_lng = std::stod(row[5]);
            r.earliest_pickup = timeStringToMin(row[6]);
            r.latest_drop = timeStringToMin(row[7]);
            if (r.latest_drop - r.earliest_pickup < 0) r.latest_drop += 1440;
        } catch (...) {
            continue;
        }

        r.vehicle_pref = row[8];
        std::transform(r.vehicle_pref.begin(), r.vehicle_pref.end(), r.vehicle_pref.begin(), ::tolower);
        if (r.vehicle_pref == "premium") r.pref_premium = true;

        r.sharing_pref = row[9];
        std::transform(r.sharing_pref.begin(), r.sharing_pref.end(), r.sharing_pref.begin(), ::tolower);
        r.max_sharing = allowedLoad(r.sharing_pref);

        requests.push_back(r);
    }
    std::cout << "Loaded " << requests.size() << " requests from " << filename << "\n";
    return requests;
}

// Build the O(1) Matrix
void buildFastMatrix(std::vector<Vehicle> &vehicles, std::vector<Request> &requests) {
    int total_nodes = vehicles.size() + requests.size() + 1;
    fast_distance.assign(total_nodes, std::vector<double>(total_nodes, 0.0));

    std::vector<std::string> id_to_str(total_nodes);
    int current_id = 0;

    for (auto &v : vehicles) {
        v.matrix_id = current_id;
        id_to_str[current_id++] = v.original_id;
    }
    for (auto &r : requests) {
        r.matrix_id = current_id;
        id_to_str[current_id++] = r.original_id;
    }

    office_matrix_id = current_id;
    id_to_str[current_id++] = "OFFICE";

    for (int i = 0; i < total_nodes; ++i) {
        for (int j = 0; j < total_nodes; ++j) {
            fast_distance[i][j] = getDistanceFromMatrix(id_to_str[i], id_to_str[j]);
        }
    }
    std::cout << "Fast O(1) distance matrix built successfully.\n";
}

// 4. CORE LOGIC (VNS)

class Route {
   public:
    int vehicle_index;
    std::vector<Node> sequence;
    std::vector<int> served_employees;

    Route(int v_idx) : vehicle_index(v_idx) {}

    RouteMetrics evaluate(const std::vector<Vehicle> &vehicles, const std::vector<Request> &requests,
                          const Config &config, bool update_times = false) {
        const Vehicle &veh = vehicles[vehicle_index];

        int current_id = veh.matrix_id;
        double current_time = veh.available_from;

        double total_dist = 0.0;
        double total_passenger_time = 0.0;
        int current_load = 0;

        std::vector<double> pickup_times(requests.size(), 0.0);
        std::vector<int> onboard;
        onboard.reserve(veh.capacity + 2);

        RouteMetrics m = {true, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

        for (auto &node : sequence) {
            const Request &req = requests[node.emp_index];
            int target_id = (node.type == PICKUP) ? req.matrix_id : office_matrix_id;
            double dist = fast_distance[current_id][target_id];
            double travel_time;
            if (veh.avg_speed_kmph <= 0) {
                if (dist < 0.005)
                    travel_time = 0;
                else
                    travel_time = 1e7;
            } else {
                travel_time = (dist / veh.avg_speed_kmph) * 60.0;
            }

            total_dist += dist;
            current_time += travel_time;

            if (node.type == PICKUP) {
                if (current_time < req.earliest_pickup) current_time = req.earliest_pickup;
            }

            if (update_times) node.arrival_time = current_time;

            if (node.type == PICKUP) {
                pickup_times[node.emp_index] = current_time;
                current_load++;
                onboard.push_back(node.emp_index);

                if (current_load > veh.capacity) m.capacity_violation += (current_load - veh.capacity);
                if (req.pref_premium && !veh.is_premium) m.capacity_violation += 10000;
            } else {
                double drop_time = current_time;
                current_load--;

                // PERF: swap-with-back + pop_back instead of find + erase (avoids shifting)
                auto it = std::find(onboard.begin(), onboard.end(), node.emp_index);
                if (it != onboard.end()) {
                    *it = onboard.back();
                    onboard.pop_back();
                }

                int buffer = (req.priority < (int)config.max_delays.size()) ? config.max_delays[req.priority] : 0;

                if (drop_time > req.latest_drop + buffer)
                    m.time_window_violation += (drop_time - (req.latest_drop + buffer));

                double direct_dist = fast_distance[req.matrix_id][office_matrix_id];
                double direct_time;
                if (veh.avg_speed_kmph <= 0) {
                    if (direct_dist < 0.005)
                        direct_time = 0;
                    else
                        direct_time = 1e7;
                } else {
                    direct_time = (direct_dist / veh.avg_speed_kmph) * 60.0;
                }

                double actual_ride_time = drop_time - pickup_times[node.emp_index];
                double delay = actual_ride_time - direct_time;

                int max_allowed_delay =
                    (req.priority < (int)config.max_delays.size()) ? config.max_delays[req.priority] : 999;

                if (delay > max_allowed_delay) m.ride_time_violation += (delay - max_allowed_delay);

                total_passenger_time += actual_ride_time;
            }

            int max_allowed_sharing = veh.capacity;
            for (int e_idx : onboard) {
                max_allowed_sharing = std::min(max_allowed_sharing, requests[e_idx].max_sharing);
            }
            if (current_load > max_allowed_sharing) m.capacity_violation += (current_load - max_allowed_sharing);

            current_id = target_id;
        }

        if (current_load != 0) m.capacity_violation += std::abs(current_load) * 100;

        m.total_cost = total_dist * veh.cost_per_km;
        m.total_time = total_passenger_time;
        m.total_dist = total_dist;

        double base_obj = (config.cost_weight * m.total_cost) + (config.time_weight * m.total_time);
        double penalties = (config.alpha * m.ride_time_violation) + (config.beta * m.time_window_violation) +
                           (config.gamma * m.capacity_violation);

        m.objective_score = base_obj + penalties;
        m.feasible = (m.ride_time_violation < 1.0 && m.time_window_violation < 1.0 && m.capacity_violation < 1.0);

        return m;
    }
};

class Solution {
   public:
    std::vector<Route> routes;
    std::vector<int> unassigned_requests;
    double total_score;
    bool feasible;

    Solution() : total_score(INF), feasible(false) {}

    void calculateTotalScore(const std::vector<Vehicle> &vehicles, const std::vector<Request> &requests,
                             const Config &config) {
        double score = 0;
        bool all_routes_feasible = true;

        for (auto &route : routes) {
            if (route.sequence.empty()) continue;

            RouteMetrics m = route.evaluate(vehicles, requests, config);
            score += m.objective_score;
            if (!m.feasible) all_routes_feasible = false;
        }
        score += unassigned_requests.size() * 1e12;
        total_score = score;
        feasible = all_routes_feasible && unassigned_requests.empty();
    }
};

class VNSSolver {
    std::vector<Request> requests;
    std::vector<Vehicle> vehicles;
    Config config;
    std::mt19937 rng;

    std::vector<double> pickup_times_buf;
    std::vector<int> onboard_buf;
    Route scratch_route;

    RouteMetrics evaluateRoute(Route &route, bool update_times = false) {
        const Vehicle &veh = vehicles[route.vehicle_index];

        int current_id = veh.matrix_id;
        double current_time = veh.available_from;

        double total_dist = 0.0;
        double total_passenger_time = 0.0;
        int current_load = 0;

        pickup_times_buf.assign(requests.size(), 0.0);
        onboard_buf.clear();

        RouteMetrics m = {true, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

        for (auto &node : route.sequence) {
            const Request &req = requests[node.emp_index];
            int target_id = (node.type == PICKUP) ? req.matrix_id : office_matrix_id;
            double dist = fast_distance[current_id][target_id];
            double travel_time;
            if (veh.avg_speed_kmph <= 0) {
                if (dist < 0.005)
                    travel_time = 0;
                else
                    travel_time = 1e7;
            } else {
                travel_time = (dist / veh.avg_speed_kmph) * 60.0;
            }

            total_dist += dist;
            current_time += travel_time;

            if (node.type == PICKUP) {
                if (current_time < req.earliest_pickup) current_time = req.earliest_pickup;
            }

            if (update_times) node.arrival_time = current_time;

            if (node.type == PICKUP) {
                pickup_times_buf[node.emp_index] = current_time;
                current_load++;
                onboard_buf.push_back(node.emp_index);

                if (current_load > veh.capacity) m.capacity_violation += (current_load - veh.capacity);
                if (req.pref_premium && !veh.is_premium) m.capacity_violation += 10000;
            } else {
                double drop_time = current_time;
                current_load--;

                auto it = std::find(onboard_buf.begin(), onboard_buf.end(), node.emp_index);
                if (it != onboard_buf.end()) {
                    *it = onboard_buf.back();
                    onboard_buf.pop_back();
                }

                int buffer = (req.priority < (int)config.max_delays.size()) ? config.max_delays[req.priority] : 0;

                if (drop_time > req.latest_drop + buffer)
                    m.time_window_violation += (drop_time - (req.latest_drop + buffer));

                double direct_dist = fast_distance[req.matrix_id][office_matrix_id];
                double direct_time;
                if (veh.avg_speed_kmph <= 0) {
                    if (dist < 0.005)
                        direct_time = 0;
                    else
                        direct_time = 1e7;
                } else {
                    direct_time = (dist / veh.avg_speed_kmph) * 60.0;
                }

                double actual_ride_time = drop_time - pickup_times_buf[node.emp_index];
                double delay = actual_ride_time - direct_time;

                int max_allowed_delay =
                    (req.priority < (int)config.max_delays.size()) ? config.max_delays[req.priority] : 999;

                if (delay > max_allowed_delay) m.ride_time_violation += (delay - max_allowed_delay);

                total_passenger_time += actual_ride_time;
            }

            int max_allowed_sharing = veh.capacity;
            for (int e_idx : onboard_buf) {
                max_allowed_sharing = std::min(max_allowed_sharing, requests[e_idx].max_sharing);
            }
            if (current_load > max_allowed_sharing) m.capacity_violation += (current_load - max_allowed_sharing);

            current_id = target_id;
        }

        if (current_load != 0) m.capacity_violation += std::abs(current_load) * 100;

        m.total_cost = total_dist * veh.cost_per_km;
        m.total_time = total_passenger_time;
        m.total_dist = total_dist;

        double base_obj = (config.cost_weight * m.total_cost) + (config.time_weight * m.total_time);
        double penalties = (config.alpha * m.ride_time_violation) + (config.beta * m.time_window_violation) +
                           (config.gamma * m.capacity_violation);

        m.objective_score = base_obj + penalties;
        m.feasible = (m.ride_time_violation < 1.0 && m.time_window_violation < 1.0 && m.capacity_violation < 1.0);

        return m;
    }

   public:
    VNSSolver(std::vector<Request> e, std::vector<Vehicle> v, Config c)
        : requests(e), vehicles(v), config(c), scratch_route(0) {
        rng = std::mt19937(static_cast<unsigned int>(time(0)));
        pickup_times_buf.resize(requests.size(), 0.0);
        onboard_buf.reserve(16);
    }

    void repair(Solution &sol) {
        for (int k = 0; k < 2; k++) {
            for (size_t r_idx = 0; r_idx < sol.routes.size(); ++r_idx) {
                Route &route = sol.routes[r_idx];
                RouteMetrics m = evaluateRoute(route);

                if (!m.feasible && !route.served_employees.empty()) {
                    std::uniform_int_distribution<int> dist(0, route.served_employees.size() - 1);
                    int emp_to_remove = route.served_employees[dist(rng)];

                    std::vector<Node> new_seq;
                    for (auto &n : route.sequence)
                        if (n.emp_index != emp_to_remove) new_seq.push_back(n);
                    route.sequence = new_seq;

                    auto it = std::find(route.served_employees.begin(), route.served_employees.end(), emp_to_remove);
                    if (it != route.served_employees.end()) {
                        *it = route.served_employees.back();
                        route.served_employees.pop_back();
                    }
                    sol.unassigned_requests.push_back(emp_to_remove);
                }
            }
        }

        std::vector<int> unassigned = sol.unassigned_requests;
        sol.unassigned_requests.clear();
        std::shuffle(unassigned.begin(), unassigned.end(), rng);

        for (int emp_idx : unassigned) {
            double best_insertion_cost = INF;
            int best_r = -1;

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route &cur = sol.routes[r];
                double base_score = evaluateRoute(cur).objective_score;

                cur.sequence.push_back({PICKUP, emp_idx, 0, 0});
                cur.sequence.push_back({DROP, emp_idx, 0, 0});
                RouteMetrics m = evaluateRoute(cur);
                cur.sequence.pop_back();
                cur.sequence.pop_back();

                if (m.objective_score < best_insertion_cost) {
                    best_insertion_cost = m.objective_score;
                    best_r = r;
                }
            }

            if (best_r != -1) {
                sol.routes[best_r].sequence.push_back({PICKUP, emp_idx, 0, 0});
                sol.routes[best_r].sequence.push_back({DROP, emp_idx, 0, 0});
                sol.routes[best_r].served_employees.push_back(emp_idx);
            } else {
                sol.unassigned_requests.push_back(emp_idx);
            }
        }
    }

    Solution constructionHeuristic() {
        Solution sol;
        for (size_t i = 0; i < vehicles.size(); ++i) sol.routes.emplace_back(i);

        std::vector<int> sorted_indices(requests.size());
        for (size_t i = 0; i < requests.size(); ++i) sorted_indices[i] = i;
        std::sort(sorted_indices.begin(), sorted_indices.end(),
                  [&](int a, int b) { return requests[a].earliest_pickup < requests[b].earliest_pickup; });

        for (int emp_idx : sorted_indices) {
            double best_score = INF;
            int best_r = -1;
            int best_i = -1, best_j = -1;

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route &cur = sol.routes[r];
                int n = cur.sequence.size();
                for (int i = 0; i <= n; ++i) {
                    cur.sequence.insert(cur.sequence.begin() + i, {PICKUP, emp_idx, 0, 0});
                    for (int j = i + 1; j <= n + 1; ++j) {
                        cur.sequence.insert(cur.sequence.begin() + j, {DROP, emp_idx, 0, 0});
                        RouteMetrics m = evaluateRoute(cur);
                        if (m.objective_score < best_score) {
                            best_score = m.objective_score;
                            best_r = r;
                            best_i = i;
                            best_j = j;
                        }
                        cur.sequence.erase(cur.sequence.begin() + j);
                    }
                    cur.sequence.erase(cur.sequence.begin() + i);
                }
            }

            if (best_r != -1) {
                sol.routes[best_r].sequence.insert(sol.routes[best_r].sequence.begin() + best_i,
                                                   {PICKUP, emp_idx, 0, 0});
                sol.routes[best_r].sequence.insert(sol.routes[best_r].sequence.begin() + best_j, {DROP, emp_idx, 0, 0});
                sol.routes[best_r].served_employees.push_back(emp_idx);
            } else {
                sol.unassigned_requests.push_back(emp_idx);
            }
        }
        sol.calculateTotalScore(vehicles, requests, config);
        return sol;
    }

    void localSearch(Solution &sol) {
        for (auto &route : sol.routes) {
            if (route.sequence.size() < 4) continue;
            bool improved = true;
            while (improved) {
                improved = false;
                double current_score = evaluateRoute(route).objective_score;

                for (size_t emp_i = 0; emp_i < route.served_employees.size(); ++emp_i) {
                    int emp = route.served_employees[emp_i];

                    scratch_route.vehicle_index = route.vehicle_index;
                    scratch_route.sequence.clear();
                    for (auto &n : route.sequence)
                        if (n.emp_index != emp) scratch_route.sequence.push_back(n);

                    int n = scratch_route.sequence.size();
                    for (int i = 0; i <= n; ++i) {
                        scratch_route.sequence.insert(scratch_route.sequence.begin() + i, {PICKUP, emp, 0, 0});
                        for (int j = i + 1; j <= n + 1; ++j) {
                            scratch_route.sequence.insert(scratch_route.sequence.begin() + j, {DROP, emp, 0, 0});
                            double s = evaluateRoute(scratch_route).objective_score;
                            if (s < current_score - 1e-5) {
                                route.sequence = scratch_route.sequence;
                                current_score = s;
                                improved = true;
                            }
                            scratch_route.sequence.erase(scratch_route.sequence.begin() + j);
                        }
                        scratch_route.sequence.erase(scratch_route.sequence.begin() + i);
                    }
                }
            }
        }
    }

    void shaking(Solution &sol, int k) {
        for (int op = 0; op < k; ++op) {
            std::vector<int> current_active;
            for (size_t i = 0; i < sol.routes.size(); ++i) {
                if (!sol.routes[i].served_employees.empty()) current_active.push_back(i);
            }
            if (current_active.empty()) break;

            std::uniform_int_distribution<int> active_dist(0, current_active.size() - 1);
            int r_idx = current_active[active_dist(rng)];

            if (sol.routes[r_idx].served_employees.empty()) continue;

            std::uniform_int_distribution<int> emp_dist(0, sol.routes[r_idx].served_employees.size() - 1);
            int emp = sol.routes[r_idx].served_employees[emp_dist(rng)];

            std::vector<Node> new_seq;
            for (auto &n : sol.routes[r_idx].sequence)
                if (n.emp_index != emp) new_seq.push_back(n);
            sol.routes[r_idx].sequence = new_seq;

            auto it =
                std::find(sol.routes[r_idx].served_employees.begin(), sol.routes[r_idx].served_employees.end(), emp);
            if (it != sol.routes[r_idx].served_employees.end()) {
                *it = sol.routes[r_idx].served_employees.back();
                sol.routes[r_idx].served_employees.pop_back();
            }

            std::uniform_int_distribution<int> route_dist(0, sol.routes.size() - 1);
            int dest = route_dist(rng);
            sol.routes[dest].sequence.push_back({PICKUP, emp, 0, 0});
            sol.routes[dest].sequence.push_back({DROP, emp, 0, 0});
            sol.routes[dest].served_employees.push_back(emp);
        }
    }

    Solution solve(int max_iterations, int &iterations_done) {
        Solution current_sol = constructionHeuristic();
        repair(current_sol);
        current_sol.calculateTotalScore(vehicles, requests, config);
        Solution best_sol = current_sol;
        int k = 1;
        auto st = std::chrono::high_resolution_clock::now();
        for (int iter = 0; iter < max_iterations; ++iter) {
            iterations_done = iter + 1;

            Solution s_prime = current_sol;
            shaking(s_prime, k);

            localSearch(s_prime);
            repair(s_prime);
            s_prime.calculateTotalScore(vehicles, requests, config);
            if (s_prime.total_score < current_sol.total_score) {
                current_sol = std::move(s_prime);
                k = 1;
                if (current_sol.total_score < best_sol.total_score) {
                    best_sol = current_sol;
                    config.alpha *= 1.01;
                    config.gamma *= 1.01;
                }
            } else {
                k = (k % 3) + 1;
            }

            auto cur = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = cur - st;

            if (elapsed.count() >= max_time) {
                break;
            }
        }
        return best_sol;
    }
};

// 5. MAIN

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <temp_directory_path>\n";
        return 1;
    }

    fs::path base_dir = argv[1];

    fs::path mode_path = base_dir / "mode.txt";

    std::ifstream f1(mode_path);
    f1 >> max_time;

    if (!fs::exists(base_dir)) {
        std::cerr << "Error: Directory " << base_dir << " does not exist.\n";
        return 1;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    Config config;

    fs::path metadata_path = base_dir / "metadata.csv";
    fs::path vehicles_path = base_dir / "vehicles.csv";
    fs::path employees_path = base_dir / "employees.csv";
    fs::path matrix_path = base_dir / "matrix.txt";

    std::cout << "Loading metadata from " << metadata_path << "...\n";
    loadMetadata(metadata_path.string(), config);

    std::cout << "Loading data...\n";
    std::vector<Vehicle> vehicles = loadVehicles(vehicles_path.string());

    if (!vehicles.empty()) {
        double max_cost_per_km = 0.0;
        for (const auto &v : vehicles) {
            if (v.cost_per_km > max_cost_per_km) {
                max_cost_per_km = v.cost_per_km;
            }
        }

        // Apply the dynamic penalties
        config.alpha = 500.0 * max_cost_per_km;
        config.beta = 1000.0 * max_cost_per_km;
    }
    std::vector<Request> requests = loadRequests(employees_path.string(), config.max_delays);

    if (requests.empty() || vehicles.empty()) {
        std::cerr << "Error: No data loaded. Exiting.\n";
        return 1;
    }

    N = requests.size();
    V = vehicles.size();
    int matrix_size = N + V + 1;

    std::cout << "Loading matrix from " << matrix_path << " (Expecting " << matrix_size << "x" << matrix_size
              << ")...\n";
    for (int i = 0; i < N; ++i) registerMatrixId(requests[i].original_id, i);      // E1->0, E123->1, etc.
    for (int k = 0; k < V; ++k) registerMatrixId(vehicles[k].original_id, N + k);  // V3->N+0, V7->N+1, etc.
    registerOfficeIdx(N + V);                                                      // OFFICE is always last
    registerMatrixId("OFFICE", N + V);                                             // also reachable by string
    loadMatrix(matrix_path.string(), matrix_size);

    std::cout << "Building fast lookup matrix...\n";
    buildFastMatrix(vehicles, requests);

    std::cout << "=== Running VNS Solver ===\n";
    VNSSolver solver(requests, vehicles, config);
    int iterations_done = 0;

    Solution final_solution = solver.solve(100000, iterations_done);
    double final_base_obj = 0.0;
    for (auto &r : final_solution.routes) {
        if (r.served_employees.empty()) continue;

        RouteMetrics m = r.evaluate(vehicles, requests, config);
        final_base_obj += (config.cost_weight * m.total_cost) + (config.time_weight * m.total_time);
    }

    std::cout << "Final Objective: " << std::fixed << std::setprecision(1) << final_base_obj << "\n";
    std::cout << "Final Total Score (With Penalties): " << std::fixed << std::setprecision(1)
              << final_solution.total_score << "\n";
    std::cout << "Unassigned Employees: " << final_solution.unassigned_requests.size() << "\n";
    std::cout << "Iterations: " << iterations_done << "\n";

    std::cout << "Generating CSV files...\n";

    fs::path emp_out_path = base_dir / "Heterogeneous_DARP" / "output_employees.csv";
    fs::path veh_out_path = base_dir / "Heterogeneous_DARP" / "output_vehicle.csv";

    fs::create_directories(emp_out_path.parent_path());

    std::ofstream emp_file(emp_out_path);
    std::ofstream veh_file(veh_out_path);

    if (!emp_file.is_open() || !veh_file.is_open()) {
        std::cerr << "Error: Could not open output files for writing at " << base_dir << "\n";
        return 1;
    }

    std::cout << "Writing to: " << emp_out_path << "\n";
    std::cout << "Writing to: " << veh_out_path << "\n";

    // Calculate the total penalty
    double total_penalty = final_solution.total_score - final_base_obj;

    emp_file << "employee_id,pickup_time,drop_time\n";
    // Write the base objective and penalty as the first line in the vehicle file
    veh_file << std::fixed << std::setprecision(1) << final_base_obj << "," << total_penalty << "\n";
    veh_file << "vehicle_id,category,employee_id,pickup_time,drop_time\n";

    for (size_t i = 0; i < final_solution.routes.size(); ++i) {
        auto &r = final_solution.routes[i];
        if (r.served_employees.empty()) continue;

        r.evaluate(vehicles, requests, config, true);  // Finalize times

        std::vector<std::string> pickup_times(requests.size());
        for (const auto &node : r.sequence) {
            std::string time_str = minToTime((int)node.arrival_time);

            if (node.type == PICKUP) {
                pickup_times[node.emp_index] = time_str;
            } else if (node.type == DROP) {
                std::string p_time = pickup_times[node.emp_index];
                std::string d_time = time_str;
                std::string e_id = requests[node.emp_index].original_id;
                std::string v_id = vehicles[i].original_id;
                std::string v_cat = capitalize(vehicles[i].category);

                veh_file << v_id << "," << v_cat << "," << e_id << "," << p_time << "," << d_time << "\n";
                emp_file << e_id << "," << p_time << "," << d_time << "\n";
            }
        }
    }

    emp_file.close();
    veh_file.close();
    std::cout << "Successfully created 'output_employees.csv' and 'output_vehicle.csv'\n";

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << "Total Execution Time: " << std::fixed << std::setprecision(2) << elapsed.count() << " seconds\n";
    return 0;
}