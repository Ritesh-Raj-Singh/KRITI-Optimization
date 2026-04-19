#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

#include "init_sol.h"
#include "io.h"
#include "params.h"
#include "vns.h"

int main(int argc, char *argv[]) {
    std::string base = argv[1];

    std::string vehicle_file = base + "/vehicles.csv";
    std::string employee_file = base + "/employees.csv";
    std::string matrix_file = base + "/matrix.txt";
    std::string metadata_file = base + "/metadata.csv";
    std::string max_iter_file = base + "/mode.txt";

    long long max_iterations = Params::DEFAULT_MAX_ITERATIONS;
    long long runtime_limit_seconds = read_runtime_limit(max_iter_file);

    std::cout << "Max iterations: " << max_iterations << "\n";
    std::cout << "Runtime limit: " << runtime_limit_seconds << "s\n";

    int h = (argc > 6) ? std::atoi(argv[6]) : Params::DEFAULT_H;

    auto start = std::chrono::high_resolution_clock::now();

    DARPInstance instance;

    read_metadata(metadata_file);

    int num_vehicles = read_vehicle_data(vehicle_file, instance);

    int num_employees_expected = count_csv_data_rows(employee_file);
    int pickup_offset = num_vehicles;
    int delivery_offset = num_vehicles + num_employees_expected;

    int num_employees = read_employee_data(employee_file, instance, pickup_offset, delivery_offset);

    loadMatrix(matrix_file, instance, num_employees, num_vehicles, pickup_offset, delivery_offset);

    std::cout << "Loaded " << num_vehicles << " vehicles, " << num_employees << " employees.\n";
    std::cout << "Running VNS (max_iter=" << max_iterations << ", runtime_limit=" << runtime_limit_seconds
              << "s, h=" << h << ")...\n";

    auto current_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = current_time - start;
    double remaining_time = (double)runtime_limit_seconds - elapsed.count() - 0.2;

    Solution initial_sol = initial_solution(instance);
    Solution best = vns1(initial_sol, instance, max_iterations, h, remaining_time);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n=== Final Solution ===\n";
    std::cout << "Total Objective Cost: " << best.total_cost() << "\n";
    std::cout << "Monetary Cost  (f1): " << best.f1 << "\n";
    std::cout << "Quality Cost   (f2): " << best.f2 << "\n";
    std::cout << "Penalties      (f3): " << best.f3 << "\n";
    std::cout << "Unassigned requests: " << best.unassigned_requests.size() << "\n";

    double total_distance = 0.0;
    double total_duration = 0.0;
    double monetary_cost = 0.0;
    int active_vehicles = 0;

    std::cout << "\n--- Route Details ---\n";
    for (const Route &route : best.routes) {
        if (route.sequence.empty()) continue;

        active_vehicles++;
        total_distance += route.total_distance;
        total_duration += route.total_duration;
        monetary_cost += route.total_distance * route.vehicle->c_k;

        std::cout << "Vehicle " << route.vehicle->id << " | Depot " << route.vehicle->depot_id << " | Type "
                  << route.vehicle->type_id << " | Nodes: " << route.sequence.size()
                  << " | Dist: " << route.total_distance << " | Dur: " << route.total_duration << " | f1: " << route.f1
                  << " | f2: " << route.f2 << " | Feasible: " << (route.is_feasible ? "yes" : "NO") << "\n  Path:";
        for (int nid : route.sequence) std::cout << " " << nid;
        std::cout << "\n";
    }

    double total_ride_time = print_ride_times(best, instance);
    std::cout << "Total ride time:  " << total_ride_time << "\n";

    double objective_cost = monetary_cost * WEIGHT_COST + total_ride_time * WEIGHT_TIME;

    std::cout << "\n--- Summary ---\n";
    std::cout << "Active vehicles:  " << active_vehicles << " / " << best.routes.size() << "\n";
    std::cout << "Total distance:   " << total_distance << "\n";
    std::cout << "Total duration:   " << total_duration << "\n";
    std::cout << "Objective cost:   " << objective_cost << "\n";

    write_output_csvs(best, instance, base, objective_cost);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "Execution time:   " << duration.count() << " s\n";

    return 0;
}
