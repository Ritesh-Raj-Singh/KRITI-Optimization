#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

#include "GraphBuilder.h"
#include "Solver.h"
#include "globals.h"
#include "matrix.h"
#include "structures.h"
#include "utils.h"

std::mt19937_64 RNG(std::chrono::steady_clock::now().time_since_epoch().count());

std::string getNodeLabel(const Node &n, const std::vector<Request> &requests, const std::vector<Vehicle> &vehicles) {
    if (n.type == Node::DUMMY_START) return "Start(" + vehicles[n.vehicle_id].original_id + ")";
    if (n.type == Node::DUMMY_END) return "End(" + vehicles[n.vehicle_id].original_id + ")";
    if (n.type == Node::PICKUP) return "Pick(" + requests[n.request_id].original_id + ")";
    if (n.type == Node::DELIVERY) return "Drop(" + requests[n.request_id].original_id + ")";
    return "Unknown";
}

struct CSVTrip {
    std::string vehicle_id;
    std::string category;
    std::string employee_id;
    std::string pickup_time;
    std::string drop_time;
};

std::string getNodeLabel(const Node &n, const std::vector<Request> &requests, const std::vector<Vehicle> &vehicles,
                         int current_veh_id = -1) {
    if (n.type == Node::DUMMY_START) {
        int v_id = (current_veh_id != -1) ? current_veh_id : n.vehicle_id;
        if (v_id >= 0 && v_id < (int)vehicles.size()) return "Start(" + vehicles[v_id].original_id + ")";
        return "Start(Unknown)";
    }
    if (n.type == Node::DUMMY_END) {
        int v_id = (current_veh_id != -1) ? current_veh_id : n.vehicle_id;
        if (v_id >= 0 && v_id < (int)vehicles.size()) return "End(" + vehicles[v_id].original_id + ")";
        return "End(Unknown)";
    }

    // For Pickups/Drops, stick to the request ID
    if (n.type == Node::PICKUP) return "Pick(" + requests[n.request_id].original_id + ")";
    if (n.type == Node::DELIVERY) return "Drop(" + requests[n.request_id].original_id + ")";

    return "Unknown";
}

void printSolution(const Solver::Solution &sol, const std::vector<Vehicle> &vehicles,
                   const std::vector<Request> &requests, GraphBuilder &gb, double dist_weight, double time_weight,
                   fs::path base_dir) {
    std::cout << "\n=============================================================\n";
    std::cout << "                  OPTIMIZED SCHEDULE SUMMARY\n";
    std::cout << "=============================================================\n";
    std::cout << "Unserved Requests:    " << sol.unserved_requests.size() << "\n";
    std::cout << "-------------------------------------------------------------\n\n";

    double grand_total_dist = 0;
    double grand_total_duration = 0;
    double grand_total_passenger_time = 0;
    double grand_total_monetary_cost = 0;

    std::vector<CSVTrip> vehicle_csv_rows;
    std::vector<CSVTrip> employee_csv_rows;

    for (auto const &[veh_id, route_nodes] : sol.routes) {
        const Vehicle &v = vehicles[veh_id - 1];

        if (route_nodes.size() <= 2) continue;

        std::unordered_map<int, std::string> pickup_time_of_request;
        std::unordered_map<int, double> passenger_pickup_time;

        std::cout << "VEHICLE " << v.original_id << " (" << (v.category == CATEGORY_PREMIUM ? "Premium" : "Normal")
                  << ") | Rate: " << v.cost_per_km << "/km\n";

        std::cout << "  " << std::left << std::setw(20) << "TIME WINDOW" << std::left << std::setw(20) << "ACTIVITY"
                  << std::left << std::setw(15) << "DIST to NEXT"
                  << "NOTES"
                  << "\n";
        std::cout << "  " << std::string(70, '-') << "\n";

        double vehicle_total_dist = 0.0;
        double start_shift_time = -1;
        double current_time = -1;

        for (size_t i = 0; i < route_nodes.size(); ++i) {
            int u_id = route_nodes[i];
            const Node &curr_node = gb.nodes[u_id];

            if (i == 0) {
                current_time = std::max((double)v.available_from, (double)curr_node.earliest_time);

                if (route_nodes.size() > 1) {
                    int next_id = route_nodes[1];
                    const Node &next_node = gb.nodes[next_id];

                    if (next_node.type != Node::DUMMY_END) {
                        std::string c1 = curr_node.getMatrixId(requests, vehicles);
                        std::string c2 = next_node.getMatrixId(requests, vehicles);

                        double dist_km = getDistanceFromMatrix(c1, c2);
                        double speed = (v.avg_speed_kmh > 0) ? v.avg_speed_kmh : 30.0;
                        double travel_time_to_first = (dist_km / speed) * 60.0;

                        if (current_time + travel_time_to_first < next_node.earliest_time) {
                            current_time = next_node.earliest_time - travel_time_to_first;
                        }
                    }
                }

                start_shift_time = current_time;
            } else {
                int prev_id = route_nodes[i - 1];
                const Node &prev_node = gb.nodes[prev_id];

                std::string c1 = prev_node.getMatrixId(requests, vehicles);
                std::string c2 = curr_node.getMatrixId(requests, vehicles);

                double dist_km = getDistanceFromMatrix(c1, c2);
                double speed = (v.avg_speed_kmh > 0) ? v.avg_speed_kmh : 30.0;
                double travel_min = (dist_km / speed) * 60.0;

                current_time += travel_min;
                vehicle_total_dist += dist_km;
            }

            // 2. Check Wait Time
            double arrival_time = current_time;
            double wait_time = 0;
            if (arrival_time < curr_node.earliest_time) {
                wait_time = curr_node.earliest_time - arrival_time;
                current_time = curr_node.earliest_time;
            }

            // 3. Service Duration
            double start_service = current_time;
            double end_service = start_service + curr_node.service_duration;
            std::string time_range = minToTimeStr((int)start_service) + " - " + minToTimeStr((int)end_service);

            // --- CSV Logic ---
            if (curr_node.type == Node::PICKUP) {
                passenger_pickup_time[curr_node.request_id] = start_service;
                pickup_time_of_request[curr_node.request_id] = minToTimeStr((int)start_service);
            } else if (curr_node.type == Node::DELIVERY) {
                if (passenger_pickup_time.find(curr_node.request_id) != passenger_pickup_time.end()) {
                    double ride_time = start_service - passenger_pickup_time[curr_node.request_id];
                    grand_total_passenger_time += ride_time;
                }
                auto it = pickup_time_of_request.find(curr_node.request_id);
                if (it != pickup_time_of_request.end()) {
                    CSVTrip row;
                    row.vehicle_id = v.original_id;
                    row.category = (v.category == CATEGORY_PREMIUM ? "Premium" : "Normal");
                    row.employee_id = requests[curr_node.request_id].original_id;
                    row.pickup_time = it->second;
                    row.drop_time = minToTimeStr((int)start_service);
                    vehicle_csv_rows.push_back(row);
                    employee_csv_rows.push_back(row);
                    pickup_time_of_request.erase(it);
                }
            }

            // 4. Print Row
            std::string label = getNodeLabel(curr_node, requests, vehicles, veh_id - 1);

            std::string notes = "";
            if (wait_time > 1.0) notes += "Wait " + std::to_string((int)wait_time) + "m ";
            if (curr_node.type == Node::PICKUP)
                notes += "[Window: " + minToTimeStr(curr_node.earliest_time) + "]";
            else if (curr_node.type == Node::DELIVERY)
                notes += "[Deadline: " + minToTimeStr(curr_node.latest_time) + "]";

            std::string dist_str = "--";
            if (i < route_nodes.size() - 1) {
                int next_id = route_nodes[i + 1];
                const Node &next_node = gb.nodes[next_id];
                if (next_node.type == Node::DUMMY_END)
                    dist_str = "0.00 km";
                else {
                    std::string c1 = curr_node.getMatrixId(requests, vehicles);
                    std::string c2 = next_node.getMatrixId(requests, vehicles);

                    double d = getDistanceFromMatrix(c1, c2);
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(2) << d << " km";
                    dist_str = oss.str();
                }
            }

            std::cout << "  " << std::left << std::setw(20) << time_range << std::left << std::setw(20) << label
                      << std::left << std::setw(15) << dist_str << notes << "\n";

            current_time = end_service;
        }

        double vehicle_duration = current_time - start_shift_time;

        double vehicle_cost = vehicle_total_dist * v.cost_per_km;

        grand_total_dist += vehicle_total_dist;
        grand_total_duration += vehicle_duration;
        grand_total_monetary_cost += vehicle_cost;

        std::cout << "  " << std::string(70, '-') << "\n";
        std::cout << "  Vehicle Stats: " << std::fixed << std::setprecision(2) << vehicle_total_dist << " km | "
                  << (int)vehicle_duration << " min | "
                  << "Cost: " << vehicle_cost << "\n\n";
    }

    // --- FINAL SUMMARY TABLE ---
    std::cout << "=============================================================\n";
    std::cout << "                  FINAL METRICS BREAKDOWN                    \n";
    std::cout << "=============================================================\n";

    // 1. Total Distance
    std::cout << std::left << std::setw(30) << "Total Distance:" << std::fixed << std::setprecision(2)
              << grand_total_dist << " km\n";

    // 2. Total Time
    std::cout << std::left << std::setw(30) << "Total Time (Duration):" << (int)grand_total_duration << " min\n";

    // 3. Total Passenger Time
    std::cout << std::left << std::setw(30) << "Total Passenger Ride Time:" << (int)grand_total_passenger_time
              << " min\n";

    // 4. Total Cost
    std::cout << std::left << std::setw(30) << "TOTAL COST:" << std::fixed << std::setprecision(2)
              << grand_total_monetary_cost << "\n";

    std::cout << "=============================================================\n";

    double final_objective = (grand_total_monetary_cost * dist_weight) + (grand_total_passenger_time * time_weight);

    std::cout << "WEIGHTED OBJECTIVE FUNCTION:\n"
              << "  (" << grand_total_monetary_cost << " * " << dist_weight << ") + "
              << "(" << grand_total_passenger_time << " * " << time_weight << ") = " << final_objective << "\n";

    std::cout << "=============================================================\n";

    // Write CSVs
    {
        fs::path veh_out_path = base_dir / "Branch-And-Cut/output_vehicle.csv";
        std::ofstream fout(veh_out_path);
        fout << costt << "," << pen << "\n";
        fout << "vehicle_id,category,employee_id,pickup_time,drop_time\n";
        for (auto &r : vehicle_csv_rows)
            fout << r.vehicle_id << "," << r.category << "," << r.employee_id << "," << r.pickup_time << ","
                 << r.drop_time << "\n";
        fout.close();
    }
    {
        fs::path emp_out_path = base_dir / "Branch-And-Cut/output_employees.csv";
        std::ofstream fout(emp_out_path);
        fout << "employee_id,pickup_time,drop_time\n";
        for (auto &r : employee_csv_rows) fout << r.employee_id << "," << r.pickup_time << "," << r.drop_time << "\n";
        fout.close();
    }
    std::cout << "\nCSV Files Generated.\n";
}

std::vector<Vehicle> loadVehicles(const std::string &filename) {
    std::vector<Vehicle> vehicles;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << filename << "\n";
        exit(1);
    }

    std::string line;
    std::getline(file, line);

    int sequential_id = 1;
    int nn = 0;
    tot_speed_per_km = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> row;
        std::cout << line << std::endl;
        while (std::getline(ss, token, ',')) {
            row.push_back(token);
        }

        if (row.size() < 8) continue;

        Vehicle v;
        v.id = sequential_id++;
        v.original_id = row[0];

        std::string catStr = row[9];
        if (catStr == "premium" || catStr == "Premium") {
            v.category = CATEGORY_PREMIUM;
        } else if (catStr == "normal" || catStr == "Normal") {
            v.category = CATEGORY_NORMAL;
        } else {
            v.category = CATEGORY_ANY;
        }

        v.max_capacity = std::stoi(row[3]);
        v.start_loc = {std::stod(row[6]), std::stod(row[7])};
        v.available_from = timeStringToMin(row[8]);
        v.cost_per_km = std::stod(row[4]);
        v.avg_speed_kmh = std::stod(row[5]);

        vehicles.push_back(v);
        tot_speed_per_km = std::max(v.cost_per_km, tot_speed_per_km);
        nn++;
    }
    avg_speed_per_km = tot_speed_per_km;
    std::cout << "Loaded " << vehicles.size() << " vehicles from " << filename << "\n";
    return vehicles;
}

std::vector<Request> loadRequests(const std::string &filename, const std::map<int, int> &priority_delays) {
    std::vector<Request> requests;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << filename << "\n";
        exit(1);
    }

    std::string line;
    std::getline(file, line);

    int sequential_id = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> row;

        while (std::getline(ss, token, ',')) {
            row.push_back(token);
        }

        if (row.size() < 10) continue;

        std::string emp_id = row[0];
        int priority = std::stoi(row[1]);
        double p_lat = std::stod(row[2]);
        double p_lng = std::stod(row[3]);
        double d_lat = std::stod(row[4]);
        double d_lng = std::stod(row[5]);
        std::string t_early = row[6];
        std::string t_late = row[7];
        std::string v_pref = row[8];

        std::string s_pref = row[9];

        Request r = createRequest(sequential_id++, emp_id, priority, p_lat, p_lng, d_lat, d_lng, t_early, t_late,
                                  v_pref, s_pref, priority_delays);
        requests.push_back(r);
    }
    std::cout << "Loaded " << requests.size() << " requests from " << filename << "\n";
    return requests;
}

void loadMetadata(const std::string &filename, std::map<int, int> &delays, double &dist_w, double &time_w) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open " << filename << ". Using defaults.\n";
        return;
    }
    std::string line;
    std::getline(file, line);  // Skip header

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string key, valStr;
        std::getline(ss, key, ',');
        std::getline(ss, valStr, ',');

        // Clean strings
        key.erase(std::remove(key.begin(), key.end(), '\r'), key.end());
        valStr.erase(std::remove(valStr.begin(), valStr.end(), '\r'), valStr.end());

        if (key == "priority_1_max_delay_min")
            delays[1] = std::stoi(valStr);
        else if (key == "priority_2_max_delay_min")
            delays[2] = std::stoi(valStr);
        else if (key == "priority_3_max_delay_min")
            delays[3] = std::stoi(valStr);
        else if (key == "priority_4_max_delay_min")
            delays[4] = std::stoi(valStr);
        else if (key == "priority_5_max_delay_min")
            delays[5] = std::stoi(valStr);
        else if (key == "objective_cost_weight")
            dist_w = std::stod(valStr);
        else if (key == "objective_time_weight")
            time_w = std::stod(valStr);
    }
    std::cout << "Metadata loaded: Cost Weight=" << dist_w << ", Time Weight=" << time_w << "\n";
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <base_directory>\n";
        return 1;
    }

    std::srand(std::time(nullptr));
    std::map<int, int> priority_delays = {{1, 10}, {2, 20}, {3, 30}, {4, 45}, {5, 60}};
    double dist_cost = 1.0;
    double time_cost = 0.0;

    fs::path base_dir = argv[1];

    if (!fs::exists(base_dir)) {
        std::cerr << "Error: Directory does not exist: " << base_dir << "\n";
        return 1;
    }

    fs::path metadata_path = base_dir / "metadata.csv";
    fs::path vehicles_path = base_dir / "vehicles.csv";
    fs::path employees_path = base_dir / "employees.csv";
    fs::path matrix_path = base_dir / "matrix.txt";
    fs::path mode_path = base_dir / "mode.txt";

    std::ifstream f1(mode_path);
    f1 >> max_time;

    loadMetadata(metadata_path.string(), priority_delays, dist_cost, time_cost);

    std::vector<Vehicle> vehicles = loadVehicles(vehicles_path.string());
    std::vector<Request> requests = loadRequests(employees_path.string(), priority_delays);

    N = requests.size();
    V = vehicles.size();
    for (int i = 0; i < N; ++i) registerMatrixId(requests[i].original_id, i);
    for (int k = 0; k < V; ++k) registerMatrixId(vehicles[k].original_id, N + k);
    registerOfficeIdx(N + V);
    registerMatrixId("OFFICE", N + V);

    loadMatrix(matrix_path.string(), N + V + 1);
    auto begin = std::chrono::high_resolution_clock::now();

    if (vehicles.empty() || requests.empty()) {
        std::cerr << "Error: Data load failed. Ensure vehicles.csv and requests.csv exist and have data.\n";
        return 1;
    }
    std::cout << "Initializing Graph...\n";
    GraphBuilder graph(requests, vehicles);

    std::cout << "Running Solver...\n";
    Solver solver(requests, vehicles, graph);

    solver.dist_cost = dist_cost;
    solver.time_cost = time_cost;
    tm_cost = time_cost;

    solver.max_iterations = 500000;

    Solver::Solution solution = solver.solveDeterministicAnnealing();

    printSolution(solution, vehicles, requests, graph, solver.dist_cost, solver.time_cost, base_dir);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
    std::cerr << "Time measured: " << elapsed.count() * 1e-9 << " seconds.\n";

    return 0;
}