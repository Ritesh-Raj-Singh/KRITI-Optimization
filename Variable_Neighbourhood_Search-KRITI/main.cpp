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

// Ensure M_PI is defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace std;

// --- Constants ---
const double R_EARTH = 6371.0;
const double INF = numeric_limits<double>::max();
const double PENALTY_UNASSIGNED = 1000000.0;
const double PENALTY_CONSTRAINT = 100000.0;

// Global Settings (Loaded from metadata.csv)
double WEIGHT_COST = 0.7;
double WEIGHT_TIME = 0.3;
map<int, int> PRIORITY_DELAYS; // Stores max delay per priority level

struct Point {
    double lat, lng;
};

// --- Helper Functions ---

// Calculate Haversine distance in km
double haversine(Point p1, Point p2) {
    double phi1 = p1.lat * M_PI / 180.0;
    double phi2 = p2.lat * M_PI / 180.0;
    double dphi = (p2.lat - p1.lat) * M_PI / 180.0;
    double dlambda = (p2.lng - p1.lng) * M_PI / 180.0;
    double a = sin(dphi / 2) * sin(dphi / 2) +
        cos(phi1) * cos(phi2) * sin(dlambda / 2) * sin(dlambda / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    return R_EARTH * c;
}

// Parse "HH:MM" string to minutes from midnight
int parseTime(string t_str) {
    if(t_str.empty()) return 0;
    int h, m;
    char colon;
    stringstream ss(t_str);
    ss >> h >> colon >> m;
    return h * 60 + m;
}

// Format minutes to "HH:MM"
string formatTime(int minutes) {
    int h = minutes / 60;
    int m = minutes % 60;
    stringstream ss;
    ss << setfill('0') << setw(2) << h << ":" << setw(2) << m;
    return ss.str();
}

// Split string by delimiter
vector<string> split(const string &s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while(getline(tokenStream, token, delimiter)) {
        if(!token.empty() && token.back() == '\r') token.pop_back(); // Handle Windows line endings
        tokens.push_back(token);
    }
    return tokens;
}

// --- Data Structures ---

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

// --- File Loading ---

void loadMetadata() {
    ifstream file("TestCase_TC01 - metadata.csv");
    if(!file.is_open()) {
        cerr << "Error: Could not open metadata.csv" << endl;
        return;
    }
    string line;
    getline(file, line); // Skip header
    while(getline(file, line)) {
        vector<string> row = split(line, ',');
        if(row.size() < 2) continue;
        string key = row[0];
        string val = row[1];

        if(key == "objective_cost_weight") WEIGHT_COST = stod(val);
        else if(key == "objective_time_weight") WEIGHT_TIME = stod(val);
        else if(key.find("priority_") != string::npos && key.find("_max_delay_min") != string::npos) {
            // Extract priority number from "priority_1_max_delay_min"
            int p = stoi(key.substr(9, 1));
            PRIORITY_DELAYS[p] = stoi(val);
        }
    }
    file.close();
}

void loadEmployees() {
    ifstream file("TestCase_TC01 - employees.csv");
    if(!file.is_open()) {
        cerr << "Error: Could not open employees.csv" << endl;
        return;
    }
    string line;
    getline(file, line); // Skip header
    while(getline(file, line)) {
        vector<string> row = split(line, ',');
        if(row.size() < 10) continue;

        Request r;
        r.id = row[0];
        r.priority = stoi(row[1]);
        r.pickup = { stod(row[2]), stod(row[3]) };
        r.drop = { stod(row[4]), stod(row[5]) };
        r.e_pickup = parseTime(row[6]);
        r.l_drop = parseTime(row[7]);
        r.veh_pref = row[8];
        r.share_pref = row[9];

        requests[r.id] = r;
        req_ids.push_back(r.id);
    }
    file.close();
}

void loadVehicles() {
    ifstream file("TestCase_TC01 - vehicles.csv");
    if(!file.is_open()) {
        cerr << "Error: Could not open vehicles.csv" << endl;
        return;
    }
    string line;
    getline(file, line); // Skip header
    while(getline(file, line)) {
        vector<string> row = split(line, ',');
        if(row.size() < 10) continue;

        Vehicle v;
        v.id = row[0];
        v.capacity = stoi(row[3]);
        v.cost_per_km = stod(row[4]);
        v.speed_km_min = stod(row[5]) / 60.0; // Convert km/h to km/min
        v.start_loc = { stod(row[6]), stod(row[7]) };
        v.start_time = parseTime(row[8]);
        v.category = row[9];

        vehicles[v.id] = v;
        veh_ids.push_back(v.id);
    }
    file.close();
}

// --- Logic & Optimization ---

EvalResult evaluateRoute(const string &v_id, const vector<Step> &route) {
    Vehicle v = vehicles[v_id];
    EvalResult res = { 0.0, 0.0, 0.0 };

    double current_time = v.start_time;
    Point current_loc = v.start_loc;
    int current_load = 0;
    set<string> passengers_on_board;
    map<string, double> pickup_times;

    for(const auto &step : route) {
        Request r = requests[step.req_id];
        Point target = (step.type == 1) ? r.pickup : r.drop;

        double dist = haversine(current_loc, target);
        double travel_time = dist / v.speed_km_min;
        double arrival_time = current_time + travel_time;
        double start_service_time = arrival_time;

        if(step.type == 1) { // Pickup
            start_service_time = max(arrival_time, (double)r.e_pickup);
            pickup_times[r.id] = start_service_time;
            current_load++;
            passengers_on_board.insert(r.id);

            // Constraint: Vehicle Preference
            if(r.veh_pref != "any" && r.veh_pref != v.category)
                res.penalty += PENALTY_CONSTRAINT;

        }
        else { // Drop
            if(pickup_times.count(r.id)) {
                res.passenger_time += (start_service_time - pickup_times[r.id]);
            }
            current_load--;
            passengers_on_board.erase(r.id);

            // Constraint: Priority-based Max Delay
            int max_delay = PRIORITY_DELAYS[r.priority];
            double max_allowed_time = r.l_drop + max_delay;

            if(start_service_time > max_allowed_time) {
                res.penalty += PENALTY_CONSTRAINT + (start_service_time - max_allowed_time) * 1000;
            }
        }

        // Constraint: Capacity
        if(current_load > v.capacity) res.penalty += PENALTY_CONSTRAINT;

        // Constraint: Sharing Preference
        for(const string &pid : passengers_on_board) {
            string pref = requests[pid].share_pref;
            int max_pax = (pref == "single") ? 1 : (pref == "double") ? 2 : 3;
            if(passengers_on_board.size() > max_pax) res.penalty += PENALTY_CONSTRAINT;
        }

        res.monetary_cost += dist * v.cost_per_km;
        current_time = start_service_time;
        current_loc = target;
    }
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

    total_penalty += sol.unassigned.size() * PENALTY_UNASSIGNED;

    // Objective: Weighted Sum
    double obj = (WEIGHT_COST * total_cost) + (WEIGHT_TIME * total_time);
    sol.obj_value = obj + total_penalty;
    return sol.obj_value;
}

Solution initialSolution() {
    Solution sol;
    for(const string &vid : veh_ids) sol.routes[vid] = {};
    sol.unassigned = req_ids;

    // Sort by earliest pickup
    sort(sol.unassigned.begin(), sol.unassigned.end(), [](const string &a, const string &b) {
        return requests[a].e_pickup < requests[b].e_pickup;
        });

    vector<string> pending = sol.unassigned;
    sol.unassigned.clear();

    for(const string &rid : pending) {
        double best_obj = INF;
        string best_veh = "";
        vector<Step> best_route;

        for(const string &vid : veh_ids) {
            vector<Step> current_route = sol.routes[vid];
            // Try every valid insertion position
            for(int i = 0; i <= current_route.size(); i++) {
                for(int j = i + 1; j <= current_route.size() + 1; j++) {
                    vector<Step> temp = current_route;
                    temp.insert(temp.begin() + i, { 1, rid });
                    temp.insert(temp.begin() + j, { 2, rid });

                    EvalResult res = evaluateRoute(vid, temp);
                    if(res.penalty == 0) { // Only consider strict feasible moves initially
                        Solution temp_sol = sol;
                        temp_sol.routes[vid] = temp;
                        double obj = calculateObjective(temp_sol);

                        if(obj < best_obj) {
                            best_obj = obj;
                            best_veh = vid;
                            best_route = temp;
                        }
                    }
                }
            }
        }
        if(best_veh != "") sol.routes[best_veh] = best_route;
        else sol.unassigned.push_back(rid);
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

int main() {
    srand(time(0));

    cout << "Loading CSV files..." << endl;
    loadMetadata();
    loadEmployees();
    loadVehicles();

    if(req_ids.empty() || veh_ids.empty()) {
        cerr << "Error: No data loaded. Check filenames (employees.csv, vehicles.csv)." << endl;
        return 1;
    }

    cout << "Loaded " << req_ids.size() << " requests, " << veh_ids.size() << " vehicles." << endl;
    cout << "Weights -> Cost: " << WEIGHT_COST << ", Time: " << WEIGHT_TIME << endl;

    Solution current = initialSolution();
    Solution best = current;

    int max_iter = 100000;
    int k_max = 2;

    for(int iter = 0; iter < max_iter; iter++) {
        int k = 1;
        while(k <= k_max) {
            Solution neighbor = current;
            if(k == 1) neighbor = neighborMove(current);
            else neighbor = neighborSwap(current);

            // Acceptance
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

    double final_cost = 0, final_time = 0;
    for(auto const &entry : best.routes) {
        EvalResult res = evaluateRoute(entry.first, entry.second);
        final_cost += res.monetary_cost;
        final_time += res.passenger_time;
    }

    // --- Output Generation ---

    // 1. Vehicle-wise Output CSV
    ofstream outFileVeh("vehicle_output.csv");
    outFileVeh << "vehicle_id,category,employee_id,pickup_time,drop_time" << endl;

    // Temporary storage for Employee-wise Output to avoid re-calculation
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
        Point cur_loc = v.start_loc;
        map<string, string> pickup_times_str;

        // Simulate route to capture exact times
        map<string, int> p_times;
        map<string, int> d_times;

        for(const auto &step : best.routes[vid]) {
            Request r = requests[step.req_id];
            Point target = (step.type == 1) ? r.pickup : r.drop;
            double dist = haversine(cur_loc, target);
            double travel = dist / v.speed_km_min;
            double arrival = cur_time + travel;

            if(step.type == 1) { // Pickup
                double start = max(arrival, (double)r.e_pickup);
                p_times[r.id] = (int)start;
                cur_time = start;
            }
            else { // Drop
                d_times[r.id] = (int)arrival;
                cur_time = arrival;
            }
            cur_loc = target;
        }

        // Write to file (Fixing the loop to be compatible with C++11)
        for(auto const &entry : p_times) {
            string eid = entry.first;
            int p_time = entry.second;
            int d_time = d_times[eid];

            outFileVeh << vid << "," << v.category << "," << eid << ","
                << formatTime(p_time) << "," << formatTime(d_time) << endl;

            // Save for employee output
            empRecords[eid] = { eid, formatTime(p_time), formatTime(d_time) };
        }
    }
    outFileVeh.close();
    cout << "Generated vehicle_output.csv" << endl;

    // 2. Employee-wise Output CSV
    ofstream outFileEmp("employee_output.csv");
    outFileEmp << "employee_id,pickup_time,drop_time" << endl;
    for(const auto &entry : empRecords) {
        outFileEmp << entry.second.emp_id << ","
            << entry.second.pickup_time << ","
            << entry.second.drop_time << endl;
    }
    outFileEmp.close();
    cout << "Generated employee_output.csv" << endl;

    return 0;
}