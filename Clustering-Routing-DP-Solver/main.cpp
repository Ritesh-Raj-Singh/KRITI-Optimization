#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <map>





using namespace std;

vector<vector<double>> DIST_MATRIX;
int MATRIX_SIZE = 0;
bool USE_MATRIX = false;
void loadMatrix(const string &filename, int size)
{
    ifstream file(filename);
    if (!file.is_open())
    {
        cerr << "Error: Could not open matrix file " << filename << "\n";
        exit(1);
    }

    DIST_MATRIX.assign(size, vector<double>(size));

    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++)
            file >> DIST_MATRIX[i][j];

    MATRIX_SIZE = size;
    USE_MATRIX = true;

    cout << "Distance matrix loaded: " << size << " x " << size << "\n";
}

/**
 * =========================================================
 * SECTION 1: SYSTEM CONFIGURATION & UTILITIES
 * =========================================================
 */
const double EARTH_RADIUS_KM = 6371.0;
const double INF = 1e18;
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


// GLOBAL CONFIGURATION
struct Metadata {
    double obj_cost_weight = 0.5; 
    double obj_time_weight = 0.5; 
    map<int, double> priority_extensions;
};

Metadata GLOBAL_CONFIG;

double deg2rad(double deg) {
    return deg * (M_PI / 180.0);
}


// double haversine(double lat1, double lon1, double lat2, double lon2) {
//     double dLat = deg2rad(lat2 - lat1);
//     double dLon = deg2rad(lon2 - lon1);
//     lat1 = deg2rad(lat1);
//     lat2 = deg2rad(lat2);
//     double a = sin(dLat / 2) * sin(dLat / 2) +
//                cos(lat1) * cos(lat2) * sin(dLon / 2) * sin(dLon / 2);
//     return EARTH_RADIUS_KM * 2 * atan2(sqrt(a), sqrt(1 - a));
// }
double haversine(double lat1, double lon1, double lat2, double lon2)
{
    if (USE_MATRIX)
    {
        // We assume lat1 and lat2 are actually being used as indices
        int i = (int)lat1;
        int j = (int)lat2;

        if (i >= 0 && i < MATRIX_SIZE && j >= 0 && j < MATRIX_SIZE)
            return DIST_MATRIX[i][j];
    }

    // Fallback to geographic calculation
    double dLat = deg2rad(lat2 - lat1);
    double dLon = deg2rad(lon2 - lon1);
    lat1 = deg2rad(lat1);
    lat2 = deg2rad(lat2);

    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(lat1) * cos(lat2) * sin(dLon / 2) * sin(dLon / 2);

    return EARTH_RADIUS_KM * 2 * atan2(sqrt(a), sqrt(1 - a));
}


double get_travel_time(double dist_km, double speed_kmh) {
    if (speed_kmh <= 0.1) return INF; 
    return (dist_km / speed_kmh) * 60.0;
}

int timeToMin(string timeStr) {
    try {
        if (timeStr.empty()) return 0;
        size_t colonPos = timeStr.find(':');
        if (colonPos == string::npos) return 0;
        int h = stoi(timeStr.substr(0, colonPos));
        int m = stoi(timeStr.substr(colonPos + 1, 2));
        return h * 60 + m;
    } catch (...) {
        return 0;
    }
}

string minToTime(int mins) {
    int h = mins / 60;
    int m = mins % 60;
    string hs = (h < 10 ? "0" : "") + to_string(h);
    string ms = (m < 10 ? "0" : "") + to_string(m);
    return hs + ":" + ms;
}

string normalize(string s) {
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (!s.empty() && s.back() == '\r') s.pop_back();
    size_t first = s.find_first_not_of(' ');
    if (string::npos == first) return s;
    size_t last = s.find_last_not_of(' ');
    return s.substr(first, (last - first + 1));
}

int shareMap(string type) {
    string t = normalize(type);
    if (t.find("single") != string::npos) return 1;
    if (t.find("double") != string::npos) return 2;
    return 3; 
}

vector<string> splitCSVLine(const string& s) {
    vector<string> tokens;
    string token;
    bool inQuotes = false;
    for (char c : s) {
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            tokens.push_back(token);
            token.clear();
        } else {
            token += c;
        }
    }
    tokens.push_back(token);
    return tokens;
}



/**
 * =========================================================
 * SECTION 2: DATA STRUCTURE DEFINITIONS
 * =========================================================
 */
struct Passenger {
    string id;
    double p_lat, p_lon;
    double d_lat, d_lon;
    double earliest_pickup;
    double latest_drop;
    int capacity_pref;      
    string vehicle_pref;    
    int priority;           
};

struct Vehicle {
    string id;
    double lat, lon;
    int capacity;
    double available_time;
    string category;        
    double avg_speed;
    double cost_per_km;     
};

struct DPNode {
    double lat, lon;
    double early;
    double late;
};

struct RouteResult {
    double cost_dist;       
    double cost_money;      
    double passenger_time;  
    double weighted_score;  
    double finish_time;
    bool valid;
};

/**
 * =========================================================
 * SECTION 3: METADATA LOADER
 * =========================================================
 */
void loadMetadata(string filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Warning: Metadata file " << filename << " not found. Using defaults." << endl;
        GLOBAL_CONFIG.priority_extensions[1] = 5;
        GLOBAL_CONFIG.priority_extensions[5] = 30;
        return;
    }

    string line;
    while (getline(file, line)) {
        if(line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        
        vector<string> cols = splitCSVLine(line);
        if (cols.size() < 2) continue;

        string key = normalize(cols[0]);
        string val_str = cols[1];
        double val = 0.0;
        try { val = stod(val_str); } catch(...) { continue; }

        if (key.find("objective_cost_weight") != string::npos) {
            GLOBAL_CONFIG.obj_cost_weight = val;
        } else if (key.find("objective_time_weight") != string::npos) {
            GLOBAL_CONFIG.obj_time_weight = val;
        } else if (key.find("priority_") != string::npos && key.find("_max_delay_min") != string::npos) {
            size_t first_us = key.find('_');
            size_t second_us = key.find('_', first_us + 1);
            if (first_us != string::npos && second_us != string::npos) {
                string prio_s = key.substr(first_us + 1, second_us - first_us - 1);
                try {
                    int prio = stoi(prio_s);
                    GLOBAL_CONFIG.priority_extensions[prio] = val;
                } catch(...) {}
            }
        }
    }
    cout << "Metadata Loaded: Cost Weight=" << GLOBAL_CONFIG.obj_cost_weight 
         << ", Time Weight=" << GLOBAL_CONFIG.obj_time_weight << endl;
}

/**
 * =========================================================
 * SECTION 4: OPTIMIZED ROUTING ENGINE (WEIGHTED DP)
 * =========================================================
 */
class RoutingEngineCommonDrop {
    int N, NODES;
    vector<vector<double>> dp; // Now stores WEIGHTED COST, not just distance
    vector<vector<double>> time_state;
    vector<vector<int>> parent;
    vector<vector<double>> dist_state; // New: to track actual distance for valid checks
    vector<DPNode> nodes;
    DPNode common_drop;
    double current_vehicle_speed;
    double current_vehicle_cost_per_km;
    int cluster_size;

    double dist_lookup(int i, int j) {
        return haversine(nodes[i].lat, nodes[i].lon, nodes[j].lat, nodes[j].lon);
    }

    void solve(int mask, int u) {
        for (int v = 1; v < NODES; v++) {
            if (!(mask & (1 << v))) {
                double d = dist_lookup(u, v);
                double travel_t = get_travel_time(d, current_vehicle_speed);
                double arrival = time_state[mask][u] + travel_t;
                double wait = max(0.0, nodes[v].early - arrival);
                double actual_time = arrival + wait;

                if (actual_time > nodes[v].late) continue;

                // --- WEIGHTED COST CALCULATION ---
                // Money Cost: Distance * Rate * Weight
                double step_money_cost = d * current_vehicle_cost_per_km;
                double step_money_weighted = step_money_cost * GLOBAL_CONFIG.obj_cost_weight;
                
                // Time Cost: Time elapsed on this leg * Number of people waiting for drop * Weight
                double time_added = travel_t + wait;
                double step_time_weighted = time_added * cluster_size * GLOBAL_CONFIG.obj_time_weight;

                double newWeightedCost = dp[mask][u] + step_money_weighted + step_time_weighted;
                
                // FIX: Defined newMask here before using it in the if-condition
                int newMask = mask | (1 << v);

                if (newWeightedCost < dp[newMask][v]) {
                    dp[newMask][v] = newWeightedCost;
                    time_state[newMask][v] = actual_time;
                    dist_state[newMask][v] = dist_state[mask][u] + d; // Track raw distance
                    parent[newMask][v] = u;
                    solve(newMask, v);
                }
            }
        }
    }

public:
    RouteResult calculate_optimal_route(Vehicle v, vector<Passenger>& cluster) {
        N = cluster.size();
        NODES = N + 1;
        cluster_size = N;
        current_vehicle_speed = v.avg_speed;
        current_vehicle_cost_per_km = v.cost_per_km;

        common_drop = {cluster[0].d_lat, cluster[0].d_lon, 0, INF};
        
        double global_drop_deadline = INF;
        for(auto& p : cluster) global_drop_deadline = min(global_drop_deadline, p.latest_drop);

        nodes.resize(NODES);
        nodes[0] = {v.lat, v.lon, v.available_time, INF};
        for(int i=0; i<N; i++) {
            nodes[i+1] = {cluster[i].p_lat, cluster[i].p_lon, cluster[i].earliest_pickup, cluster[i].latest_drop};
        }

        dp.assign(1 << NODES, vector<double>(NODES, INF));
        time_state.assign(1 << NODES, vector<double>(NODES, INF));
        dist_state.assign(1 << NODES, vector<double>(NODES, INF));
        parent.assign(1 << NODES, vector<int>(NODES, -1));

        dp[1][0] = 0;
        time_state[1][0] = v.available_time;
        dist_state[1][0] = 0;

        solve(1, 0);

        RouteResult best_res = {INF, INF, INF, INF, INF, false};
        int finalMask = (1 << NODES) - 1;

        for (int i = 1; i < NODES; i++) {
            if (dp[finalMask][i] < INF) {
                // Calculate Drop Leg
                double dist_to_drop = haversine(nodes[i].lat, nodes[i].lon, common_drop.lat, common_drop.lon);
                double travel_t = get_travel_time(dist_to_drop, current_vehicle_speed);
                double final_arrival_time = time_state[finalMask][i] + travel_t;

                if (final_arrival_time <= global_drop_deadline) {
                    double total_trip_dist = dist_state[finalMask][i] + dist_to_drop;
                    double trip_monetary_cost = total_trip_dist * v.cost_per_km;
                    
                    double total_passenger_time = 0;
                    for(auto& p : cluster) {
                        total_passenger_time += (final_arrival_time - p.earliest_pickup);
                    }

                    // Recalculate final weighted score to ensure exact consistency
                    double weighted_score = (trip_monetary_cost * GLOBAL_CONFIG.obj_cost_weight) + 
                                            (total_passenger_time * GLOBAL_CONFIG.obj_time_weight);

                    if (weighted_score < best_res.weighted_score) {
                        best_res.cost_dist = total_trip_dist;
                        best_res.cost_money = trip_monetary_cost;
                        best_res.passenger_time = total_passenger_time;
                        best_res.weighted_score = weighted_score;
                        best_res.finish_time = final_arrival_time;
                        best_res.valid = true;
                    }
                }
            }
        }
        return best_res;
    }

    pair<vector<pair<int, double>>, double> get_schedule(Vehicle v, vector<Passenger>& cluster) {
        // Re-run with updated weights to ensure state is fresh
        calculate_optimal_route(v, cluster);

        int NODES = cluster.size() + 1;
        int finalMask = (1 << NODES) - 1;
        int bestEndNode = -1;
        double best_weighted = INF;
        double bestDropTime = INF;
        DPNode drop = {cluster[0].d_lat, cluster[0].d_lon, 0, INF};

        for (int i = 1; i < NODES; i++) {
             if (dp[finalMask][i] < INF) {
                // Manually calc final score to pick best end node from DP results
                double d = haversine(nodes[i].lat, nodes[i].lon, drop.lat, drop.lon);
                double t = time_state[finalMask][i] + get_travel_time(d, v.avg_speed);
                
                double total_dist = dist_state[finalMask][i] + d;
                double cost = total_dist * v.cost_per_km;
                double p_time = 0;
                for(int k=0; k<(int)cluster.size(); k++) p_time += (t - nodes[k+1].early); 
                
                double score = (cost * GLOBAL_CONFIG.obj_cost_weight) + (p_time * GLOBAL_CONFIG.obj_time_weight);

                if(score < best_weighted) { 
                    best_weighted = score; 
                    bestEndNode = i; 
                    bestDropTime = t; 
                }
             }
        }

        vector<int> path;
        int curr = bestEndNode;
        int mask = finalMask;
        while(curr != 0 && curr != -1) {
            path.push_back(curr);
            int prev = parent[mask][curr];
            mask ^= (1 << curr);
            curr = prev;
        }
        reverse(path.begin(), path.end());

        vector<pair<int, double>> pickups;
        double curr_t = v.available_time;
        int prev_node_idx = 0;

        for(int node_idx : path) {
            double d = haversine(nodes[prev_node_idx].lat, nodes[prev_node_idx].lon,
                                 nodes[node_idx].lat, nodes[node_idx].lon);
            double arr = curr_t + get_travel_time(d, v.avg_speed);
            double actual = max(arr, nodes[node_idx].early);

            pickups.push_back({node_idx - 1, actual});
            curr_t = actual;
            prev_node_idx = node_idx;
        }
        
        double d_drop = haversine(nodes[prev_node_idx].lat, nodes[prev_node_idx].lon, drop.lat, drop.lon);
        bestDropTime = curr_t + get_travel_time(d_drop, v.avg_speed);

        return {pickups, bestDropTime};
    }
};

/**
 * =========================================================
 * SECTION 5: CLUSTERING HELPERS & PREFERENCE LOGIC
 * =========================================================
 */

bool are_compatible_vehicle_pref(string p1, string p2) {
    p1 = normalize(p1);
    p2 = normalize(p2);
    if (p1 == "any" || p2 == "any") return true;
    return p1 == p2;
}

double get_cost_single(Passenger p) {
    return haversine(p.p_lat, p.p_lon, p.d_lat, p.d_lon);
}

double get_batch_cost_common_drop(Passenger a, Passenger b) {
    double d1 = haversine(a.p_lat, a.p_lon, b.p_lat, b.p_lon) +
                haversine(b.p_lat, b.p_lon, a.d_lat, a.d_lon);
    double d2 = haversine(b.p_lat, b.p_lon, a.p_lat, a.p_lon) +
                haversine(a.p_lat, a.p_lon, a.d_lat, a.d_lon);
    return min(d1, d2);
}

double calculate_dissimilarity(Passenger a, Passenger b) {
    if (a.capacity_pref == 1 || b.capacity_pref == 1) return 1e9; 
    if (!are_compatible_vehicle_pref(a.vehicle_pref, b.vehicle_pref)) return 1e9;

    double time_diff = abs(a.earliest_pickup - b.earliest_pickup);
    double time_penalty = time_diff / 60.0; // In Hours

    double cost_i = get_cost_single(a);
    double cost_j = get_cost_single(b);
    double cost_ij = get_batch_cost_common_drop(a, b);
    
    double spatial_score = 0;
    if ((cost_i + cost_j) > 1e-9) {
        spatial_score = cost_ij / (cost_i + cost_j);
    }

    return (spatial_score * GLOBAL_CONFIG.obj_cost_weight) + 
           (time_penalty * GLOBAL_CONFIG.obj_time_weight);
}

double get_avg_dis(int u, const vector<int>& cluster, const vector<vector<double>>& adj) {
    if (cluster.empty()) return 0.0;
    double sum = 0; int count = 0;
    for (int v : cluster) { if (u != v) { sum += adj[u][v]; count++; } }
    return count > 0 ? sum / count : 0.0;
}

double get_avg_dis_cross(int u, const vector<int>& cluster, const vector<vector<double>>& adj) {
    if (cluster.empty()) return 1e9;
    double sum = 0; for (int v : cluster) sum += adj[u][v];
    return sum / cluster.size();
}

double calculate_silhouette(const vector<vector<int>>& clusters, const vector<vector<double>>& adj) {
    double total_score = 0; int count = 0;
    for (size_t i = 0; i < clusters.size(); i++) {
        if (clusters[i].size() <= 1) continue;
        for (int u : clusters[i]) {
            double a = get_avg_dis(u, clusters[i], adj);
            double b = 1e9;
            for (size_t j = 0; j < clusters.size(); j++) {
                if (i == j) continue;
                double val = get_avg_dis_cross(u, clusters[j], adj);
                b = min(b, val);
            }
            if (b >= 1e8) b = a + 1;
            total_score += (b - a) / max(a, b);
            count++;
        }
    }
    return count > 0 ? total_score / count : -1.0;
}

/**
 * =========================================================
 * SECTION 6: DATA LOADING
 * =========================================================
 */

vector<Passenger> loadPassengers(string filename) {
    vector<Passenger> passengers;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open " << filename << endl;
        return passengers;
    }
    string line;
    if(getline(file, line)) {}
    while (getline(file, line)) {
        if(line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        vector<string> cols = splitCSVLine(line);
        if (cols.size() < 10) continue; 
        Passenger p;
        p.id = cols[0];
        try { p.priority = stoi(cols[1]); } catch(...) { p.priority = 5; }
        try { p.p_lat = stod(cols[2]); } catch(...) { p.p_lat = 0; }
        try { p.p_lon = stod(cols[3]); } catch(...) { p.p_lon = 0; }
        try { p.d_lat = stod(cols[4]); } catch(...) { p.d_lat = 0; }
        try { p.d_lon = stod(cols[5]); } catch(...) { p.d_lon = 0; }
        p.earliest_pickup = (double)timeToMin(cols[6]);
        double base_latest = (double)timeToMin(cols[7]);
        double extension = 0;
        if (GLOBAL_CONFIG.priority_extensions.count(p.priority)) {
            extension = GLOBAL_CONFIG.priority_extensions[p.priority];
        } else {
            if (p.priority == 1) extension = 5;
            else if (p.priority == 2) extension = 10;
            else if (p.priority == 3) extension = 15;
            else if (p.priority == 4) extension = 20;
            else extension = 30;
        }
        p.latest_drop = base_latest + extension;
        p.vehicle_pref = cols[8]; 
        p.capacity_pref = shareMap(cols[9]);
        passengers.push_back(p);
    }
    return passengers;
}

vector<Vehicle> loadVehicles(string filename) {
    vector<Vehicle> vehicles;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open " << filename << endl;
        return vehicles;
    }
    string line;
    if(getline(file, line)) {}
    while (getline(file, line)) {
        if(line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        vector<string> cols = splitCSVLine(line);
        if (cols.size() < 10) continue;
        Vehicle v;
        v.id = cols[0];
        try { v.capacity = stoi(cols[3]); } catch(...) { v.capacity = 4; }
        try { v.cost_per_km = stod(cols[4]); } catch(...) { v.cost_per_km = 10.0; } 
        try { v.avg_speed = stod(cols[5]); } catch(...) { v.avg_speed = 30.0; }
        try { v.lat = stod(cols[6]); } catch(...) { v.lat = 0; }
        try { v.lon = stod(cols[7]); } catch(...) { v.lon = 0; }
        v.available_time = (double)timeToMin(cols[8]);
        v.category = cols[9];
        vehicles.push_back(v);
    }
    return vehicles;
}


/**
 * =========================================================
 * SECTION 7: MAIN
 * =========================================================
 */
int main(int argc, char **argv)
{
    if (argc < 5)
    {
        cerr << "Usage: ./program vehicles.csv employees.csv metadata.csv matrix.txt\n";
        return 1;
    }

    string vehicle_file = argv[1];
    string employee_file = argv[2];
    string metadata_file = argv[3];
    string matrix_file = argv[4];


    // Load metadata
    loadMetadata(metadata_file);

    // Load data
    vector<Vehicle> vehicles = loadVehicles(vehicle_file);
    vector<Passenger> passengers = loadPassengers(employee_file);

    if (passengers.empty() || vehicles.empty())
    {
        cerr << "Error: Data load failed.\n";
        return 1;
    }

    cout << "Loaded " << vehicles.size() << " vehicles from " << vehicle_file << "\n";
    cout << "Loaded " << passengers.size() << " employees from " << employee_file << "\n";

    sort(vehicles.begin(), vehicles.end(), [](const Vehicle &a, const Vehicle &b)
         { return a.capacity < b.capacity; });

    int n = passengers.size();
    vector<vector<double>> s_matrix(n, vector<double>(n));

    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            s_matrix[i][j] = (i == j) ? 0 : calculate_dissimilarity(passengers[i], passengers[j]);


    int N = passengers.size();
int V = vehicles.size();

int matrix_size = N + V + 5;  // or exact size used in BAC

loadMatrix(matrix_file, matrix_size);


    // ================= CLUSTERING LOGIC (UNCHANGED) =================
    vector<vector<int>> best_config;
    best_config.push_back({});
    for (int i = 0; i < n; i++)
        best_config[0].push_back(i);

    double best_score = -2.0;
    vector<vector<int>> current_clusters = best_config;

    while (true)
    {
        int target_idx = -1;
        double max_diam = -1.0;

        for (size_t i = 0; i < current_clusters.size(); i++)
        {
            if (current_clusters[i].size() <= 1)
                continue;

            double diam = 0;
            for (int u : current_clusters[i])
                for (int v : current_clusters[i])
                    diam = max(diam, s_matrix[u][v]);

            if (diam > max_diam)
            {
                max_diam = diam;
                target_idx = i;
            }
        }

        if (target_idx == -1)
            break;

        vector<int> &old_cluster = current_clusters[target_idx];

        int splinter_id = -1;
        double max_avg = -1.0;

        for (int u : old_cluster)
        {
            double avg = get_avg_dis(u, old_cluster, s_matrix);
            if (avg > max_avg)
            {
                max_avg = avg;
                splinter_id = u;
            }
        }

        vector<int> splinter_cluster;
        splinter_cluster.push_back(splinter_id);
        old_cluster.erase(remove(old_cluster.begin(), old_cluster.end(), splinter_id), old_cluster.end());

        bool changed = true;
        while (changed)
        {
            changed = false;
            for (auto it = old_cluster.begin(); it != old_cluster.end();)
            {
                if (get_avg_dis_cross(*it, splinter_cluster, s_matrix) <
                    get_avg_dis(*it, old_cluster, s_matrix))
                {
                    splinter_cluster.push_back(*it);
                    it = old_cluster.erase(it);
                    changed = true;
                }
                else
                    ++it;
            }
        }

        current_clusters.push_back(splinter_cluster);

        double score = calculate_silhouette(current_clusters, s_matrix);
        if (score > best_score)
        {
            best_score = score;
            best_config = current_clusters;
        }

        bool split = false;
        for (auto &c : current_clusters)
            if (c.size() > 1)
                split = true;

        if (!split)
            break;
    }

    // ================= ROUTING + BAC STYLE OUTPUT =================

    RoutingEngineCommonDrop router;

    double grand_total_dist = 0;
    double grand_total_passenger_time = 0;
    double grand_total_monetary_cost = 0;

    vector<tuple<string, string, string, string, string>> csv_rows_vehicle;
    vector<tuple<string, string, string>> csv_rows_employee;

    // cout << "\n=============================================================\n";
    // cout << "                  OPTIMIZED SCHEDULE SUMMARY\n";
    // cout << "=============================================================\n\n";

    for (auto &cluster : best_config)
    {
        vector<Passenger> cluster_passengers;
        for (int pid : cluster)
            cluster_passengers.push_back(passengers[pid]);

        for (auto &v : vehicles)
        {
            if ((int)cluster_passengers.size() > v.capacity)
                continue;

            RouteResult res = router.calculate_optimal_route(v, cluster_passengers);

            if (!res.valid)
                continue;

            auto schedule = router.get_schedule(v, cluster_passengers);

            // cout << "VEHICLE " << v.id
            //      << " | Rate: " << v.cost_per_km << "/km\n";

            // cout << "  DISTANCE: " << fixed << setprecision(2)
            //      << res.cost_dist << " km\n";

            // cout << "  MONEY COST: " << res.cost_money << "\n";
            // cout << "  PASSENGER TIME: " << res.passenger_time << " min\n";

            // cout << "  WEIGHTED OBJECTIVE: ("
            //      << res.cost_money << " * " << GLOBAL_CONFIG.obj_cost_weight
            //      << ") + (" << res.passenger_time << " * "
            //      << GLOBAL_CONFIG.obj_time_weight << ") = "
            //      << res.weighted_score << "\n\n";

            string drop_time_str = minToTime((int)schedule.second);

            for (auto &p_sched : schedule.first)
            {
                string emp_id = cluster_passengers[p_sched.first].id;
                string pickup_time_str = minToTime((int)p_sched.second);

                csv_rows_vehicle.push_back(
                    {v.id, v.category, emp_id,
                     pickup_time_str, drop_time_str});

                csv_rows_employee.push_back(
                    {emp_id, pickup_time_str, drop_time_str});
            }

            grand_total_dist += res.cost_dist;
            grand_total_monetary_cost += res.cost_money;
            grand_total_passenger_time += res.passenger_time;

            break;
        }
    }

    cout << "=============================================================\n";
    cout << "FINAL METRICS CLustering-DP-Routing\n";
    cout << "=============================================================\n";

    cout << "Total Distance: " << grand_total_dist << " km\n";
    cout << "Total Passenger Time: " << grand_total_passenger_time << " min\n";
    cout << "Total Monetary Cost: " << grand_total_monetary_cost << "\n";

    double final_objective =
        (grand_total_monetary_cost * GLOBAL_CONFIG.obj_cost_weight) +
        (grand_total_passenger_time * GLOBAL_CONFIG.obj_time_weight);

    cout << "WEIGHTED OBJECTIVE: "
         << final_objective << "\n";

    cout << "=============================================================\n";

    // Write CSVs
    {
        ofstream fout("output_vehicle.csv");
        fout << "vehicle_id,category,employee_id,pickup_time,drop_time\n";
        for (auto &r : csv_rows_vehicle)
            fout << get<0>(r) << "," << get<1>(r) << ","
                 << get<2>(r) << "," << get<3>(r)
                 << "," << get<4>(r) << "\n";
    }

    {
        ofstream fout("output_employees.csv");
        fout << "employee_id,pickup_time,drop_time\n";
        for (auto &r : csv_rows_employee)
            fout << get<0>(r) << ","
                 << get<1>(r) << ","
                 << get<2>(r) << "\n";
    }

    cout << "\nCSV Files Generated.\n";

    return 0;
}
