#define _USE_MATH_DEFINES
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <map>
#include <limits>
#include <random>
#include <iomanip>
#include <set>
#include <ctime>
#include <sstream>
#include <fstream>
#include <chrono>
#include "matrix.h"


#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

using namespace std;

extern int N;
extern int V;

// ==========================================
// 1. DATA STRUCTURES
// ==========================================

const double R_EARTH = 6371.0;
const double INF = numeric_limits<double>::max();

// Helper to convert minutes to HH:MM
string minToTime(int m) {
    int hrs = (m / 60) % 24;
    int mins = m % 60;
    stringstream ss;
    ss << setfill('0') << setw(2) << hrs << ":" << setw(2) << mins;
    return ss.str();
}

// Helper to convert HH:MM string to minutes
int timeStringToMin(string t_str) {
    if (t_str.empty()) return 0;
    int hrs = 0, mins = 0;
    char colon;
    stringstream ss(t_str);
    ss >> hrs >> colon >> mins;
    return hrs * 60 + mins;
}

// Helper to capitalize category
string capitalize(string s) {
    if (s.empty()) return s;
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    s[0] = toupper(s[0]);
    return s;
}

struct Config {
    double cost_weight = 1.0;
    double time_weight = 0.0;
    
    double alpha = 5000.0;   // Ride Time Penalty
    double beta = 10000.0;   // Time Window Penalty
    double gamma = 100000.0; // Capacity/Sharing Penalty
    
    // Default delays, will be overwritten by loadMetadata
    map<int, int> max_delays = {{1, 10}, {2, 20}, {3, 30}, {4, 45}, {5, 60}};
};

enum NodeType { PICKUP, DROP };

// Renamed from 'Employee' to 'Request' to match your reference naming convention
struct Request {
    int id;              // Sequential ID (0-based) used by solver
    string original_id;  // ID from CSV
    int priority;
    double pickup_lat, pickup_lng;
    double drop_lat, drop_lng;
    int earliest_pickup; 
    int latest_drop;     
    string vehicle_pref; 
    string sharing_pref; 
};

struct Vehicle {
    int id;              // Sequential ID (1-based) used by solver
    string original_id;  // ID from CSV
    int capacity;
    double cost_per_km;
    double avg_speed_kmph;
    double current_lat, current_lng;
    int available_from; 
    string category;    
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

// ==========================================
// 2. FILE LOADING FUNCTIONS (As Requested)
// ==========================================

void loadMetadata(const string &filename, Config &config) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Warning: Could not open " << filename << ". Using defaults.\n";
        return;
    }
    string line;
    getline(file, line); // Skip header

    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string key, valStr;
        getline(ss, key, ',');
        getline(ss, valStr, ',');

        // Clean strings
        key.erase(remove(key.begin(), key.end(), '\r'), key.end());
        valStr.erase(remove(valStr.begin(), valStr.end(), '\r'), valStr.end());

        if (key == "priority_1_max_delay_min") config.max_delays[1] = stoi(valStr);
        else if (key == "priority_2_max_delay_min") config.max_delays[2] = stoi(valStr);
        else if (key == "priority_3_max_delay_min") config.max_delays[3] = stoi(valStr);
        else if (key == "priority_4_max_delay_min") config.max_delays[4] = stoi(valStr);
        else if (key == "priority_5_max_delay_min") config.max_delays[5] = stoi(valStr);
        else if (key == "objective_cost_weight") config.cost_weight = stod(valStr);
        else if (key == "objective_time_weight") config.time_weight = stod(valStr);
    }
    cout << "Metadata loaded: Cost Weight=" << config.cost_weight << ", Time Weight=" << config.time_weight << "\n";
}

vector<Vehicle> loadVehicles(const string &filename) {
    vector<Vehicle> vehicles;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open " << filename << "\n";
        exit(1);
    }

    string line;
    getline(file, line); // Skip header row

    int sequential_id = 0; // 0-based index for vector access

    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string token;
        vector<string> row;

        while (getline(ss, token, ',')) {
             // Basic cleaning
            token.erase(0, token.find_first_not_of(" \t\r\n\""));
            token.erase(token.find_last_not_of(" \t\r\n\"") + 1);
            row.push_back(token);
        }

        if (row.size() < 8) continue;

        Vehicle v;
        v.id = sequential_id++;
        v.original_id = row[0];

        // Parse Category
        string catStr = row[9];
        // Normalize category string for solver logic
        if (catStr == "premium" || catStr == "Premium") v.category = "premium";
        else if (catStr == "normal" || catStr == "Normal") v.category = "normal";
        else v.category = "any";

        try {
            v.capacity = stoi(row[3]);
            v.cost_per_km = stod(row[4]);
            v.avg_speed_kmph = stod(row[5]);
            v.current_lat = stod(row[6]);
            v.current_lng = stod(row[7]);
            v.available_from = timeStringToMin(row[8]);
        } catch (...) {
            cerr << "Skipping invalid vehicle row: " << row[0] << endl;
            continue;
        }

        vehicles.push_back(v);
    }
    cout << "Loaded " << vehicles.size() << " vehicles from " << filename << "\n";
    return vehicles;
}

vector<Request> loadRequests(const string &filename, const map<int, int> &priority_delays) {
    vector<Request> requests;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open " << filename << "\n";
        exit(1);
    }

    string line;
    getline(file, line); // Skip header

    int sequential_id = 0; // Solver expects 0-based indexing for requests

    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string token;
        vector<string> row;

        while (getline(ss, token, ',')) {
             // Basic cleaning
            token.erase(0, token.find_first_not_of(" \t\r\n\""));
            token.erase(token.find_last_not_of(" \t\r\n\"") + 1);
            row.push_back(token);
        }

        // Expected CSV Format:
        // emp_id, priority, p_lat, p_lng, d_lat, d_lng, t_early, t_late, v_pref, s_pref

        if (row.size() < 10) continue;

        Request r;
        r.id = sequential_id++;
        r.original_id = row[0];

        try {
            r.priority = stoi(row[1]);
            r.pickup_lat = stod(row[2]);
            r.pickup_lng = stod(row[3]);
            r.drop_lat = stod(row[4]);
            r.drop_lng = stod(row[5]);
            r.earliest_pickup = timeStringToMin(row[6]);
            r.latest_drop = timeStringToMin(row[7]);
        } catch (...) {
             cerr << "Skipping invalid request row: " << row[0] << endl;
             continue;
        }

        r.vehicle_pref = row[8];
        transform(r.vehicle_pref.begin(), r.vehicle_pref.end(), r.vehicle_pref.begin(), ::tolower);
        
        r.sharing_pref = row[9];
        transform(r.sharing_pref.begin(), r.sharing_pref.end(), r.sharing_pref.begin(), ::tolower);

        requests.push_back(r);
    }
    cout << "Loaded " << requests.size() << " requests from " << filename << "\n";
    return requests;
}

// ==========================================
// 3. HELPER LOGIC
// ==========================================

double toRadians(double degree) { return degree * M_PI / 180.0; }

double haversine(double lat1, double lon1, double lat2, double lon2) {
    double dLat = toRadians(lat2 - lat1);
    double dLon = toRadians(lon2 - lon1);
    double a = sin(dLat / 2) * sin(dLat / 2) + cos(toRadians(lat1)) * cos(toRadians(lat2)) * sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return R_EARTH * c;
}

double calculateTravelTime(double dist_km, double speed_kmph) {
    if (speed_kmph <= 0) return 999999.0;
    return (dist_km / speed_kmph) * 60.0;
}

int allowedLoad(const string& pref) {
    string p = pref;
    if (p == "single") return 1;
    if (p == "double") return 2;
    if (p == "triple") return 3;
    return 1000; 
}

// ==========================================
// 4. CORE LOGIC (VNS)
// ==========================================

class Route {
public:
    int vehicle_index;
    vector<Node> sequence;
    set<int> served_employees;

    Route(int v_idx) : vehicle_index(v_idx) {}

    RouteMetrics evaluate(const vector<Vehicle>& vehicles, const vector<Request>& requests, const Config& config) {
        const Vehicle& veh = vehicles[vehicle_index];
        double current_lat = veh.current_lat;
        double current_lng = veh.current_lng;
        double current_time = veh.available_from;
        
        double total_dist = 0.0;
        double total_passenger_time = 0.0;
        int current_load = 0;
        
        map<int, double> pickup_times;
        set<int> onboard;
        
        RouteMetrics m = {true, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

        for (auto& node : sequence) {
            const Request& req = requests[node.emp_index];
            double target_lat = (node.type == PICKUP) ? req.pickup_lat : req.drop_lat;
            double target_lng = (node.type == PICKUP) ? req.pickup_lng : req.drop_lng;

            double dist = haversine(current_lat, current_lng, target_lat, target_lng);
            double travel_time = calculateTravelTime(dist, veh.avg_speed_kmph);

            total_dist += dist;
            current_time += travel_time;

            if (node.type == PICKUP) {
                if (current_time < req.earliest_pickup) current_time = req.earliest_pickup;
            }
            
            node.arrival_time = current_time;

            if (node.type == PICKUP) {
                pickup_times[node.emp_index] = current_time;
                current_load++;
                onboard.insert(node.emp_index);

                if (current_load > veh.capacity) m.capacity_violation += (current_load - veh.capacity);
                
                // Strict Preference Check
                if (req.vehicle_pref == "premium" && veh.category != "premium") m.capacity_violation += 10000; 

            } else { // DROP
                double drop_time = current_time;
                current_load--;
                onboard.erase(node.emp_index);

                if (drop_time > req.latest_drop) m.time_window_violation += (drop_time - req.latest_drop);

                double direct_dist = haversine(req.pickup_lat, req.pickup_lng, req.drop_lat, req.drop_lng);
                double direct_time = calculateTravelTime(direct_dist, veh.avg_speed_kmph);
                double actual_ride_time = drop_time - pickup_times[node.emp_index];
                double delay = actual_ride_time - direct_time;

                int max_allowed_delay = 999;
                if (config.max_delays.count(req.priority)) max_allowed_delay = config.max_delays.at(req.priority);

                if (delay > max_allowed_delay) m.ride_time_violation += (delay - max_allowed_delay);

                total_passenger_time += actual_ride_time;
            }
            
            int max_allowed_sharing = veh.capacity;
            for (int e_idx : onboard) {
                max_allowed_sharing = min(max_allowed_sharing, allowedLoad(requests[e_idx].sharing_pref));
            }
            if (current_load > max_allowed_sharing) m.capacity_violation += (current_load - max_allowed_sharing);

            current_lat = target_lat;
            current_lng = target_lng;
        }
        
        if (current_load != 0) m.capacity_violation += abs(current_load) * 100;

        m.total_cost = total_dist * veh.cost_per_km;
        m.total_time = total_passenger_time;
        m.total_dist = total_dist;

        double base_obj = (config.cost_weight * m.total_cost) + (config.time_weight * m.total_time);
        double penalties = (config.alpha * m.ride_time_violation) + 
                           (config.beta * m.time_window_violation) + 
                           (config.gamma * m.capacity_violation);

        m.objective_score = base_obj + penalties;
        m.feasible = (m.ride_time_violation < 1.0 && m.time_window_violation < 1.0 && m.capacity_violation < 1.0);

        return m;
    }
};

class Solution {
public:
    vector<Route> routes;
    vector<int> unassigned_requests; 
    double total_score;
    bool feasible;

    Solution() : total_score(INF), feasible(false) {}

    void calculateTotalScore(const vector<Vehicle>& vehicles, const vector<Request>& requests, const Config& config) {
        double score = 0;
        bool all_routes_feasible = true;
        
        for (auto& route : routes) {
            if (route.sequence.empty()) continue;
            Route r_copy = route; 
            RouteMetrics m = r_copy.evaluate(vehicles, requests, config);
            score += m.objective_score;
            if (!m.feasible) all_routes_feasible = false;
        }
        score += unassigned_requests.size() * 100000.0;
        total_score = score;
        feasible = all_routes_feasible && unassigned_requests.empty();
    }
};

class VNSSolver {
    vector<Request> requests;
    vector<Vehicle> vehicles;
    Config config;
    mt19937 rng;

public:
    VNSSolver(vector<Request> e, vector<Vehicle> v, Config c) 
        : requests(e), vehicles(v), config(c) {
        rng = mt19937(static_cast<unsigned int>(time(0)));
    }

    void repair(Solution& sol) {
        // Try removing infeasible parts
        for(int k=0; k<2; k++) {
            for (size_t r_idx = 0; r_idx < sol.routes.size(); ++r_idx) {
                Route& route = sol.routes[r_idx];
                RouteMetrics m = route.evaluate(vehicles, requests, config);
                
                if (!m.feasible && !route.served_employees.empty()) {
                    vector<int> emps(route.served_employees.begin(), route.served_employees.end());
                    int emp_to_remove = emps[rng() % emps.size()];
                    
                    vector<Node> new_seq;
                    for(auto& n : route.sequence) if(n.emp_index != emp_to_remove) new_seq.push_back(n);
                    route.sequence = new_seq;
                    route.served_employees.erase(emp_to_remove);
                    sol.unassigned_requests.push_back(emp_to_remove);
                }
            }
        }
        
        // Try re-inserting unassigned
        vector<int> unassigned = sol.unassigned_requests;
        sol.unassigned_requests.clear();
        shuffle(unassigned.begin(), unassigned.end(), rng);

        for (int emp_idx : unassigned) {
            double best_insertion_cost = INF;
            int best_r = -1;
            vector<Node> best_seq;
            
            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route cur = sol.routes[r];
                double base_score = cur.evaluate(vehicles, requests, config).objective_score;

                // Simple insertion at end for speed in repair
                vector<Node> temp = cur.sequence;
                temp.push_back({PICKUP, emp_idx, 0,0});
                temp.push_back({DROP, emp_idx, 0,0});
                
                cur.sequence = temp;
                RouteMetrics m = cur.evaluate(vehicles, requests, config);
                
                if (m.objective_score < base_score + 20000.0) { 
                    if (m.objective_score < best_insertion_cost) {
                        best_insertion_cost = m.objective_score;
                        best_r = r;
                        best_seq = temp;
                    }
                }
            }

            if (best_r != -1) {
                sol.routes[best_r].sequence = best_seq;
                sol.routes[best_r].served_employees.insert(emp_idx);
            } else {
                sol.unassigned_requests.push_back(emp_idx);
            }
        }
    }

    Solution constructionHeuristic() {
        Solution sol;
        for (size_t i = 0; i < vehicles.size(); ++i) sol.routes.emplace_back(i);

        vector<int> sorted_indices(requests.size());
        for(size_t i=0; i<requests.size(); ++i) sorted_indices[i] = i;
        sort(sorted_indices.begin(), sorted_indices.end(), [&](int a, int b) {
            return requests[a].earliest_pickup < requests[b].earliest_pickup;
        });

        for (int emp_idx : sorted_indices) {
            double best_score = INF;
            int best_r = -1;
            vector<Node> best_seq;

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route cur = sol.routes[r];
                
                int n = cur.sequence.size();
                for (int i = 0; i <= n; ++i) {
                    for (int j = i + 1; j <= n + 1; ++j) {
                        vector<Node> temp = cur.sequence;
                        temp.insert(temp.begin() + i, {PICKUP, emp_idx, 0,0});
                        temp.insert(temp.begin() + j, {DROP, emp_idx, 0,0});
                        
                        cur.sequence = temp;
                        RouteMetrics m = cur.evaluate(vehicles, requests, config);
                        
                        if (m.objective_score < best_score) {
                            best_score = m.objective_score;
                            best_r = r;
                            best_seq = temp;
                        }
                    }
                }
            }

            if (best_r != -1) {
                sol.routes[best_r].sequence = best_seq;
                sol.routes[best_r].served_employees.insert(emp_idx);
            } else {
                sol.unassigned_requests.push_back(emp_idx);
            }
        }
        sol.calculateTotalScore(vehicles, requests, config);
        return sol;
    }

    void localSearch(Solution& sol) {
        for (auto& route : sol.routes) {
            if (route.sequence.size() < 4) continue;
            bool improved = true;
            while (improved) {
                improved = false;
                double current_score = route.evaluate(vehicles, requests, config).objective_score;
                vector<int> emps(route.served_employees.begin(), route.served_employees.end());
                
                for (int emp : emps) {
                    vector<Node> temp_seq;
                    for(auto& n : route.sequence) if(n.emp_index != emp) temp_seq.push_back(n);

                    int n = temp_seq.size();
                    for (int i = 0; i <= n; ++i) {
                        for (int j = i + 1; j <= n + 1; ++j) {
                            vector<Node> test = temp_seq;
                            test.insert(test.begin() + i, {PICKUP, emp, 0,0});
                            test.insert(test.begin() + j, {DROP, emp, 0,0});
                            
                            Route r_test = route;
                            r_test.sequence = test;
                            double s = r_test.evaluate(vehicles, requests, config).objective_score;
                            
                            if (s < current_score - 1e-5) {
                                route.sequence = test;
                                current_score = s;
                                improved = true;
                            }
                        }
                    }
                }
            }
        }
    }

    Solution shaking(Solution sol, int k) {
        for(int op=0; op<k; ++op) {
            vector<int> current_active;
            for(size_t i=0; i<sol.routes.size(); ++i) {
                if(!sol.routes[i].served_employees.empty()) current_active.push_back(i);
            }
            if(current_active.empty()) break;

            int r_idx = current_active[rng() % current_active.size()];
            if(sol.routes[r_idx].served_employees.empty()) continue;

            auto it = sol.routes[r_idx].served_employees.begin();
            advance(it, rng() % sol.routes[r_idx].served_employees.size());
            int emp = *it;

            vector<Node> new_seq;
            for(auto& n : sol.routes[r_idx].sequence) if(n.emp_index != emp) new_seq.push_back(n);
            sol.routes[r_idx].sequence = new_seq;
            sol.routes[r_idx].served_employees.erase(emp);

            int dest = rng() % sol.routes.size();
            sol.routes[dest].sequence.push_back({PICKUP, emp, 0,0});
            sol.routes[dest].sequence.push_back({DROP, emp, 0,0});
            sol.routes[dest].served_employees.insert(emp);
        }
        return sol;
    }

    Solution solve(int max_iterations, int& iterations_done) {
        Solution current_sol = constructionHeuristic();
        repair(current_sol);
        
        current_sol.calculateTotalScore(vehicles, requests, config);
        Solution best_sol = current_sol;
        
        int k = 1;
        
        for (int iter = 0; iter < max_iterations; ++iter) {
            iterations_done = iter + 1;
            Solution s_prime = shaking(current_sol, k);
            localSearch(s_prime);
            repair(s_prime); 
            
            s_prime.calculateTotalScore(vehicles, requests, config);

            if (s_prime.total_score < current_sol.total_score) {
                current_sol = s_prime;
                k = 1;
                
                if (current_sol.total_score < best_sol.total_score) {
                    best_sol = current_sol;
                    config.alpha *= 1.01;
                    config.gamma *= 1.01;
                }
            } else {
                k = (k % 3) + 1;
            }
        }
        return best_sol;
    }
};

// ==========================================
// 6. MAIN
// ==========================================

int main(int argc, char **argv) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <vehicles.csv> <requests.csv> <metadata.csv> [matrix.csv]" << endl;
        return 1;
    }

    // START TIMER
    auto start_time = std::chrono::high_resolution_clock::now();

    Config config;
    
    // 1. Load Metadata
    cout << "Loading metadata from " << argv[3] << "..." << endl;
    loadMetadata(argv[3], config);

    // 2. Load Vehicles and Requests
    cout << "Loading data..." << endl;
    vector<Vehicle> vehicles = loadVehicles(argv[1]);
    vector<Request> requests = loadRequests(argv[2], config.max_delays);

    // 2. === CRITICAL STEP: ASSIGN VALUES TO GLOBAL VARIABLES ===
    // Now that data is loaded, update the global N and V
    N = requests.size();
    V = vehicles.size();


    // Optional: Load Matrix (placeholder for structure compliance)
    if (argc > 4) {
        cout << "Matrix file provided (logic skipped in this snippet): " << argv[4] << endl;
        // loadMatrix(argv[4], ...); 
        loadMatrix(argv[4], N + V);
    }

    if (requests.empty() || vehicles.empty()) {
        cerr << "Error: No data loaded. Exiting." << endl;
        return 1;
    }

    cout << "=== Running VNS Solver ===" << endl;
    VNSSolver solver(requests, vehicles, config);
    int iterations_done = 0;
    
    Solution final_solution = solver.solve(2000, iterations_done); 

    cout << "Final Objective: " << fixed << setprecision(1) << final_solution.total_score << endl;
    cout << "Iterations: " << iterations_done << endl;
    
    // OUTPUT GENERATION
    cout << "Generating CSV files..." << endl;
    
    ofstream emp_file("output_employees.csv");
    ofstream veh_file("output_vehicle.csv");

    emp_file << "employee_id,pickup_time,drop_time" << endl;
    veh_file << "vehicle_id,category,employee_id,pickup_time,drop_time" << endl;

    for (size_t i = 0; i < final_solution.routes.size(); ++i) {
        auto& r = final_solution.routes[i];
        if (r.served_employees.empty()) continue;

        // Force update of times
        r.evaluate(vehicles, requests, config);

        map<int, string> pickup_times;

        for (const auto& node : r.sequence) {
             string time_str = minToTime((int)node.arrival_time);
             
             if (node.type == PICKUP) {
                 pickup_times[node.emp_index] = time_str;
             } else if (node.type == DROP) {
                 string p_time = pickup_times[node.emp_index];
                 string d_time = time_str;
                 string e_id = requests[node.emp_index].original_id;
                 string v_id = vehicles[i].original_id;
                 string v_cat = capitalize(vehicles[i].category);
                 
                 veh_file << v_id << "," << v_cat << "," << e_id << "," << p_time << "," << d_time << endl;
                 emp_file << e_id << "," << p_time << "," << d_time << endl;
             }
        }
    }

    emp_file.close();
    veh_file.close();

    cout << "Successfully created 'output_employees.csv' and 'output_vehicle.csv'" << endl;
    
    if (!final_solution.unassigned_requests.empty()) {
        cout << "WARNING: " << final_solution.unassigned_requests.size() << " unassigned requests!" << endl;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    
    cout << "===========================================" << endl;
    cout << "Total Execution Time: " << fixed << setprecision(2) << elapsed.count() << " seconds" << endl;
    cout << "===========================================" << endl;
    return 0;
}