#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <set>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <random>

using namespace std;

// --- Constants ---
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const double R_EARTH = 6371.0;
const double INF = numeric_limits<double>::max();

// New Penalty Weights based on Reference
const double ALPHA = 5000.0;   // Ride Time Delay Penalty (linear)
const double BETA = 10000.0;   // Time Window Penalty (linear)
const double GAMMA = 100000.0; // Capacity/Preference Penalty (linear/fixed)

// Global Settings (Loaded from metadata.csv)
double WEIGHT_COST = 0.7;
double WEIGHT_TIME = 0.3;
map<int, int> PRIORITY_DELAYS;

// --- Matrix Definitions ---
int N = 0;
int V = 0;
std::vector<std::vector<double>> matrix;

void loadMatrix(const std::string &filename, int size) {
    matrix.assign(size, std::vector<double>(size));
    std::ifstream fin(filename);
    if(!fin) {
        std::cerr << "Cannot open matrix file\n";
        std::exit(1);
    }
    for(int i = 0; i < size; i++)
        for(int j = 0; j < size; j++)
            fin >> matrix[i][j];
}

int convert(const std::string &a) {
    if(a[0] == 'E') return std::stoi(a.substr(1)) - 1;
    if(a[0] == 'V') return N + std::stoi(a.substr(1)) - 1;
    return N + V; // OFFICE / DROP
}

double getDistanceFromMatrix(const std::string &a, const std::string &b) {
    return matrix[convert(a)][convert(b)];
}

int getTravelTimeFromMatrix(const std::string &a, const std::string &b, double speed_kmh) {
    double d = getDistanceFromMatrix(a, b);
    return (d < 0.005) ? 0 : std::ceil((d / speed_kmh) * 60.0);
}

// --- Data Structures ---

struct Point { double lat, lng; };

struct Request {
    string id;
    Point pickup;
    Point drop;
    int e_pickup;
    int l_drop;
    string veh_pref;
    string share_pref;
    int priority;
};

struct Vehicle {
    string id;
    Point start_loc;
    int capacity;
    double cost_per_km;
    double speed_km_min;
    int start_time;
    string category;
};

struct Step {
    int type; // 1=Pickup, 2=Drop
    string req_id;
};

struct Solution {
    map<string, vector<Step>> routes;
    vector<string> unassigned;
    double obj_value = 0.0;
};

struct EvalResult {
    double monetary_cost;
    double passenger_time;
    double penalty;
};

// Global Data Storage
map<string, Request> requests;
map<string, Vehicle> vehicles;
vector<string> req_ids;
vector<string> veh_ids;

// --- Helper Functions ---

int parseTime(string t_str) {
    if(t_str.empty()) return 0;
    int h, m; char colon;
    stringstream ss(t_str);
    ss >> h >> colon >> m;
    return h * 60 + m;
}

string formatTime(int minutes) {
    int h = minutes / 60;
    int m = minutes % 60;
    stringstream ss;
    ss << setfill('0') << setw(2) << h << ":" << setw(2) << m;
    return ss.str();
}

vector<string> split(const string &s, char delimiter) {
    vector<string> tokens; string token;
    istringstream tokenStream(s);
    while(getline(tokenStream, token, delimiter)) {
        if(!token.empty() && token.back() == '\r') token.pop_back();
        tokens.push_back(token);
    }
    return tokens;
}

// --- File Loading ---

void loadMetadata(const string &ok) {
    ifstream file(ok);
    if(!file.is_open()) return;
    string line;
    getline(file, line);
    while(getline(file, line)) {
        vector<string> row = split(line, ',');
        if(row.size() < 2) continue;
        string key = row[0];
        string val = row[1];

        if(key == "objective_cost_weight") WEIGHT_COST = stod(val);
        else if(key == "objective_time_weight") WEIGHT_TIME = stod(val);
        else if(key.find("priority_") != string::npos && key.find("_max_delay_min") != string::npos) {
            int p = stoi(key.substr(9, 1));
            PRIORITY_DELAYS[p] = stoi(val);
        }
    }
    file.close();
}

void loadEmployees(const string &ok) {
    ifstream file(ok);
    if(!file.is_open()) return;
    string line;
    getline(file, line);
    while(getline(file, line)) {
        vector<string> row = split(line, ',');
        if(row.size() < 10) continue;
        Request r;
        r.id = row[0]; r.priority = stoi(row[1]);
        r.pickup = { stod(row[2]), stod(row[3]) };
        r.drop = { stod(row[4]), stod(row[5]) };
        r.e_pickup = parseTime(row[6]);
        r.l_drop = parseTime(row[7]);
        r.veh_pref = row[8]; r.share_pref = row[9];
        requests[r.id] = r;
        req_ids.push_back(r.id);
    }
    N = req_ids.size();
    file.close();
}

void loadVehicles(const string &ok) {
    ifstream file(ok);
    if(!file.is_open()) return;
    string line;
    getline(file, line);
    while(getline(file, line)) {
        vector<string> row = split(line, ',');
        if(row.size() < 10) continue;
        Vehicle v;
        v.id = row[0]; v.capacity = stoi(row[3]);
        v.cost_per_km = stod(row[4]);
        v.speed_km_min = stod(row[5]) / 60.0;
        v.start_loc = { stod(row[6]), stod(row[7]) };
        v.start_time = parseTime(row[8]);
        v.category = row[9];
        vehicles[v.id] = v;
        veh_ids.push_back(v.id);
    }
    V = veh_ids.size();
    file.close();
}

// --- Logic & Optimization ---

EvalResult evaluateRoute(const string &v_id, const vector<Step> &route) {
    Vehicle v = vehicles[v_id];
    EvalResult res = { 0.0, 0.0, 0.0 };

    double current_time = v.start_time;
    string last_location_id = v_id;
    int current_load = 0;

    set<string> passengers_on_board;
    map<string, double> pickup_times;

    for(const auto &step : route) {
        Request r = requests[step.req_id];
        string target_location_id = (step.type == 1) ? step.req_id : "OFFICE";
        double dist = getDistanceFromMatrix(last_location_id, target_location_id);
        last_location_id = target_location_id;

        double travel_time = dist / v.speed_km_min;
        double arrival_time = current_time + travel_time;
        double start_service_time = arrival_time;

        if(step.type == 1) { // Pickup
            start_service_time = max(arrival_time, (double)r.e_pickup);
            pickup_times[r.id] = start_service_time;
            current_load++;
            passengers_on_board.insert(r.id);

            // Penalty: Vehicle Preference
            if(r.veh_pref != "any" && r.veh_pref != v.category)
                res.penalty += GAMMA;

        }
        else { // Drop
            if(pickup_times.count(r.id)) {
                double actual_ride_time = start_service_time - pickup_times[r.id];
                res.passenger_time += actual_ride_time;

                // Penalty: Priority-based Max Delay (Linear Ride Time Penalty)
                double direct_dist = getDistanceFromMatrix(r.id, "OFFICE");
                double direct_time = direct_dist / v.speed_km_min;
                double delay = actual_ride_time - direct_time;
                int max_allowed_delay = PRIORITY_DELAYS[r.priority];

                if(delay > max_allowed_delay) {
                    res.penalty += (delay - max_allowed_delay) * ALPHA;
                }
            }
            current_load--;
            passengers_on_board.erase(r.id);

            // Penalty: Time Window Violation (Linear)
            if(start_service_time > r.l_drop) {
                res.penalty += (start_service_time - r.l_drop) * BETA;
            }
        }

        // Penalty: Capacity Violation (Linear)
        if(current_load > v.capacity) {
            res.penalty += (current_load - v.capacity) * GAMMA;
        }

        // Penalty: Sharing Preference 
        int max_pax = v.capacity;
        for(const string &pid : passengers_on_board) {
            string pref = requests[pid].share_pref;
            int limit = (pref == "single") ? 1 : (pref == "double") ? 2 : 3;
            if(limit < max_pax) max_pax = limit;
        }
        if(passengers_on_board.size() > max_pax) {
            res.penalty += (passengers_on_board.size() - max_pax) * GAMMA;
        }

        res.monetary_cost += dist * v.cost_per_km;
        current_time = start_service_time;
    }

    // Safety check: ensure vehicle empties
    if(current_load != 0) res.penalty += abs(current_load) * GAMMA;

    return res;
}

double calculateObjective(Solution &sol) {
    double total_cost = 0;
    double total_time = 0;
    double total_penalty = 0;

    for(auto const &entry : sol.routes) {
        EvalResult res = evaluateRoute(entry.first, entry.second);
        total_cost += res.monetary_cost;
        total_time += res.passenger_time;
        total_penalty += res.penalty;
    }

    // Unassigned penalty retained conceptually, but should remain 0 based on new assignment logic
    total_penalty += sol.unassigned.size() * GAMMA;

    double obj = (WEIGHT_COST * total_cost) + (WEIGHT_TIME * total_time);
    sol.obj_value = obj + total_penalty;
    return sol.obj_value;
}

Solution initialSolution() {
    Solution sol;
    for(const string &vid : veh_ids) sol.routes[vid] = {};
    sol.unassigned.clear(); // We won't unassign anyone

    vector<string> pending = req_ids;
    // Sort by earliest pickup
    sort(pending.begin(), pending.end(), [](const string &a, const string &b) {
        return requests[a].e_pickup < requests[b].e_pickup;
        });

    for(const string &rid : pending) {
        double best_obj = INF;
        string best_veh = veh_ids[0]; // Initialize to prevent empty assignment
        vector<Step> best_route;

        for(const string &vid : veh_ids) {
            vector<Step> current_route = sol.routes[vid];
            // Try every valid insertion position
            for(int i = 0; i <= current_route.size(); i++) {
                for(int j = i + 1; j <= current_route.size() + 1; j++) {
                    vector<Step> temp = current_route;
                    temp.insert(temp.begin() + i, { 1, rid });
                    temp.insert(temp.begin() + j, { 2, rid });

                    Solution temp_sol = sol;
                    temp_sol.routes[vid] = temp;
                    double obj = calculateObjective(temp_sol);

                    // Allow assignment even with penalties to ensure ALL are routed
                    if(obj < best_obj) {
                        best_obj = obj;
                        best_veh = vid;
                        best_route = temp;
                    }
                }
            }
        }
        // Force assignment
        sol.routes[best_veh] = best_route;
    }
    calculateObjective(sol);
    return sol;
}

Solution neighborMove(Solution sol) {
    vector<string> active_vehs;
    for(auto const &entry : sol.routes) if(!entry.second.empty()) active_vehs.push_back(entry.first);
    if(active_vehs.empty()) return sol;

    string v_from = active_vehs[rand() % active_vehs.size()];
    vector<Step> &r_from = sol.routes[v_from];
    string req_to_move = r_from[rand() % r_from.size()].req_id;

    vector<Step> new_r_from;
    for(auto s : r_from) if(s.req_id != req_to_move) new_r_from.push_back(s);
    sol.routes[v_from] = new_r_from;

    string v_to = veh_ids[rand() % veh_ids.size()];
    vector<Step> r_to = sol.routes[v_to];

    int i = rand() % (r_to.size() + 1);
    r_to.insert(r_to.begin() + i, { 1, req_to_move });
    int k = i + 1 + (rand() % (r_to.size() - i));
    r_to.insert(r_to.begin() + k, { 2, req_to_move });

    sol.routes[v_to] = r_to;
    calculateObjective(sol);
    return sol;
}

Solution neighborSwap(Solution sol) {
    vector<string> active_vehs;
    for(auto const &entry : sol.routes) if(!entry.second.empty()) active_vehs.push_back(entry.first);
    if(active_vehs.size() < 2) return sol;

    string v1 = active_vehs[rand() % active_vehs.size()];
    string v2 = active_vehs[rand() % active_vehs.size()];
    while(v1 == v2) v2 = active_vehs[rand() % active_vehs.size()];

    string r1 = sol.routes[v1][rand() % sol.routes[v1].size()].req_id;
    string r2 = sol.routes[v2][rand() % sol.routes[v2].size()].req_id;

    vector<Step> rt1, rt2;
    for(auto s : sol.routes[v1]) if(s.req_id != r1) rt1.push_back(s);
    for(auto s : sol.routes[v2]) if(s.req_id != r2) rt2.push_back(s);

    int i1 = rand() % (rt1.size() + 1);
    rt1.insert(rt1.begin() + i1, { 1, r2 });
    int j1 = i1 + 1 + (rand() % (rt1.size() - i1));
    rt1.insert(rt1.begin() + j1, { 2, r2 });

    int i2 = rand() % (rt2.size() + 1);
    rt2.insert(rt2.begin() + i2, { 1, r1 });
    int j2 = i2 + 1 + (rand() % (rt2.size() - i2));
    rt2.insert(rt2.begin() + j2, { 2, r1 });

    sol.routes[v1] = rt1;
    sol.routes[v2] = rt2;
    calculateObjective(sol);
    return sol;
}

int main(int argc, char **argv) {
    if(argc < 5) {
        cerr << "Usage: ./program <vehicles.csv> <employees.csv> <metadata.csv> <matrix.txt>\n";
        return 1;
    }

    srand(time(0));

    cout << "Loading CSV files..." << endl;
    loadVehicles(argv[1]);
    loadEmployees(argv[2]);
    loadMetadata(argv[3]);
    loadMatrix(argv[4], N + V + 1);

    if(req_ids.empty() || veh_ids.empty()) {
        cerr << "Error: No data loaded. Check filenames." << endl;
        return 1;
    }

    cout << "Loaded " << req_ids.size() << " requests, " << veh_ids.size() << " vehicles." << endl;

    Solution current = initialSolution();
    Solution best = current;

    int max_iter = 10000;
    int k_max = 2;

    for(int iter = 0; iter < max_iter; iter++) {
        int k = 1;
        while(k <= k_max) {
            Solution neighbor = current;
            if(k == 1) neighbor = neighborMove(current);
            else neighbor = neighborSwap(current);

            if(neighbor.obj_value < current.obj_value) {
                current = neighbor;
                if(current.obj_value < best.obj_value) best = current;
                k = 1;
            }
            else {
                k++;
            }
        }
    }

    // --- Calculate Final Stats ---
    double final_cost = 0, final_time = 0, final_penalty = 0;
    for(auto const &entry : best.routes) {
        EvalResult res = evaluateRoute(entry.first, entry.second);
        final_cost += res.monetary_cost;
        final_time += res.passenger_time;
        final_penalty += res.penalty;
    }
    double final_obj = calculateObjective(best);

    cout << "\n=== Optimized Schedule ===" << endl;
    cout << "Final Objective Score : " << fixed << setprecision(3) << final_obj << endl;
    cout << "Total Monetary Cost   : " << final_cost << endl;
    cout << "Total Passenger Time  : " << final_time << " min" << endl;
    cout << "Total Penalty Applied : " << final_penalty << endl;
    cout << "Unassigned Requests   : " << best.unassigned.size() << endl;

    // --- Output CSVs ---
    ofstream outFileVeh("vehicle_output.csv");
    outFileVeh << fixed << setprecision(3) << final_obj << "," << final_penalty << endl;
    outFileVeh << "vehicle_id,category,employee_id,pickup_time,drop_time" << endl;

    struct EmpRecord {
        string emp_id;
        string pickup_time;
        string drop_time;
    };
    map<string, EmpRecord> empRecords;

    for(const string &vid : veh_ids) {
        if(best.routes[vid].empty()) continue;
        Vehicle v = vehicles[vid];
        double cur_time = v.start_time;
        string last_location_id = vid;
        map<string, int> p_times, d_times;

        for(const auto &step : best.routes[vid]) {
            Request r = requests[step.req_id];
            string target_location_id = (step.type == 1) ? step.req_id : "OFFICE";
            double dist = getDistanceFromMatrix(last_location_id, target_location_id);
            last_location_id = target_location_id;

            double travel = dist / v.speed_km_min;
            double arrival = cur_time + travel;

            if(step.type == 1) {
                double start = max(arrival, (double)r.e_pickup);
                p_times[r.id] = (int)start;
                cur_time = start;
            }
            else {
                d_times[r.id] = (int)arrival;
                cur_time = arrival;
            }
        }

        for(auto const &entry : p_times) {
            string eid = entry.first;
            outFileVeh << vid << "," << v.category << "," << eid << ","
                << formatTime(entry.second) << "," << formatTime(d_times[eid]) << endl;
            empRecords[eid] = { eid, formatTime(entry.second), formatTime(d_times[eid]) };
        }
    }
    outFileVeh.close();

    ofstream outFileEmp("employee_output.csv");
    outFileEmp << "employee_id,pickup_time,drop_time" << endl;
    for(const auto &entry : empRecords) {
        outFileEmp << entry.second.emp_id << "," << entry.second.pickup_time << "," << entry.second.drop_time << endl;
    }
    outFileEmp.close();

    cout << "\nOutput successfully saved to 'vehicle_output.csv', 'employee_output.csv', and 'summary.csv'." << endl;

    return 0;
}
