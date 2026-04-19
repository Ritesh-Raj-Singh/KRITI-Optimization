#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "ALNS.h"
#include "CSVReader.h"
#include "CostFunction.h"
#include "Distance.h"
#include "mapper.h"
std::map<std::pair<double, double>, int> mappy;
double path_len[251][251];

void printRouteTrace(const Route &r, const Vehicle &v, const std::vector<Employee> &emp, double &outDist,
                     double &outDuration, const Metadata &meta) {
    outDist = 0;
    outDuration = 0;
    if (r.seq.empty()) return;

    SplitResult sr = evaluateRouteDP(r.seq, v, emp, meta);
    std::vector<int> splits = sr.splits;
    splits.push_back(r.seq.size());

    double t = v.startTime;
    double t1 = 0;
    double cx = v.x, cy = v.y;
    double totalDist = 0.0;

    for (size_t i = 0; i < splits.size() - 1; i++) {
        int start = splits[i];
        int end = splits[i + 1];
        int batchSize = end - start;
        std::vector<double> pickupTimes(batchSize);

        for (int k = start; k < end; k++) {
            const auto &e = emp[r.seq[k]];
            double d = distKm(cx, cy, e.x, e.y);
            totalDist += d;
            t += (d / v.speed) * 60.0;
            double startService = std::max(t, e.ready);
            t = startService;
            pickupTimes[k - start] = t;
            cx = e.x;
            cy = e.y;

            std::cout << e.originalId << "(pickup) -> ";
        }

        const auto &last = emp[r.seq[end - 1]];
        double dOff = distKm(cx, cy, last.destX, last.destY);
        totalDist += dOff;
        t += (dOff / v.speed) * 60.0;
        cx = last.destX;
        cy = last.destY;

        for (int k = start; k < end; k++) {
            t1 += (t - pickupTimes[k - start]);
            std::cout << emp[r.seq[k]].originalId << "(drop) -> ";
        }
    }
    std::cout << "End";

    outDist = totalDist;
    outDuration = t1;
}

std::string formatTime(double mins) {
    int h = (int)(mins / 60.0);
    int m = (int)mins % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << h << ":" << std::setw(2) << std::setfill('0') << m;
    return oss.str();
}

void generateOutputFiles(const std::vector<Route> &solution, const std::vector<Vehicle> &vehicles,
                         const std::vector<Employee> &emp, const Metadata &meta, std::string dir) {
    std::string vPath = dir + "/ALNS/output_vehicle.csv";
    std::string ePath = dir + "/ALNS/output_employees.csv";

    std::ofstream vFile(vPath);
    std::ofstream eFile(ePath);

    std::cout << "Writing outputs to: " << vPath << "\n";

    double totalOpCost = 0.0;
    double totalPenalty = 0.0;

    std::vector<bool> assigned(emp.size(), false);

    for (const Route &r : solution) {
        for (int id : r.seq) assigned[id] = true;

        if (r.seq.empty()) continue;
        CostComponents cc = getRouteCostComponents(r, vehicles[r.vehicleId], emp, meta);
        totalOpCost += cc.operationalCost;
        totalPenalty += cc.penaltyCost;
    }
    int unassigned_req = 0;
    for (int i = 0; i < (int)emp.size(); i++) {
        if (!assigned[i]) unassigned_req++;
    }

    vFile << std::fixed << std::setprecision(2) << totalOpCost << "," << totalPenalty << ",";
    vFile << unassigned_req << "\n";
    vFile << "vehicle_id,category,employee_id,pickup_time,drop_time\n";
    eFile << "employee_id,pickup_time,drop_time\n";

    for (const Route &r : solution) {
        if (r.seq.empty()) continue;

        const Vehicle &v = vehicles[r.vehicleId];
        std::string cat = v.premium ? "premium" : "normal";

        SplitResult sr = evaluateRouteDP(r.seq, v, emp, meta);
        std::vector<int> splits = sr.splits;
        splits.push_back(r.seq.size());

        double t = v.startTime;
        double cx = v.x, cy = v.y;

        for (size_t i = 0; i < splits.size() - 1; i++) {
            int start = splits[i];
            int end = splits[i + 1];
            int batchSize = end - start;
            std::vector<std::string> pickTimes(batchSize);

            for (int k = start; k < end; k++) {
                const auto &e = emp[r.seq[k]];
                double d = distKm(cx, cy, e.x, e.y);
                t += (d / v.speed) * 60.0;
                double startService = std::max(t, e.ready);
                t = startService;
                pickTimes[k - start] = formatTime(t);
                cx = e.x;
                cy = e.y;
            }

            const auto &last = emp[r.seq[end - 1]];
            double dOff = distKm(cx, cy, last.destX, last.destY);
            t += (dOff / v.speed) * 60.0;
            cx = last.destX;
            cy = last.destY;

            std::string dropTimeStr = formatTime(t);

            for (int k = start; k < end; k++) {
                const auto &e = emp[r.seq[k]];
                vFile << v.originalId << "," << cat << "," << e.originalId << "," << pickTimes[k - start] << ","
                      << dropTimeStr << "\n";
                eFile << e.originalId << "," << pickTimes[k - start] << "," << dropTimeStr << "\n";
            }
        }
    }

    vFile.close();
    eFile.close();
    std::cout << "Generated output_vehicle.csv and output_employees.csv\n";
}

int main(int argc, char **argv) {
    auto start = std::chrono::high_resolution_clock::now();

    if (argc < 2) {
        std::cerr << "Usage: ./main_ALNS directory\n";
        return 1;
    }

    auto vehicles = readVehicles(argv[1] + std::string("/vehicles.csv"));
    auto employees = readEmployees(argv[1] + std::string("/employees.csv"));

    if (vehicles.empty()) {
        std::cerr << "Error: No vehicles loaded from " << argv[1] << "/vehicles.csv\n";
    }

    if (vehicles.empty() || employees.empty()) {
        std::cerr << "Error: No vehicles or employees loaded.\n";
        return 1;
    }

    Metadata meta;

    if (argc >= 2)
        meta = readMetadata(argv[1] + std::string("/metadata.csv"));
    else {
        std::cerr << "Warning: No metadata provided. Using default values.\n";
        meta.objectiveCostWeight = 1.0;
        meta.objectiveTimeWeight = 1.0;
    }
    readDist(argv[1] + std::string("/matrix.txt"), (int)(employees.size() + vehicles.size()) + 1);

    std::string mode_file = argv[1] + std::string("/mode.txt");
    std::ifstream f1(mode_file);
    f1 >> max_time;

    int idx = 0;
    for (int i = 0; i < employees.size(); i++) {
        mappy[{employees[i].x, employees[i].y}] = idx++;
    }
    for (int i = 0; i < vehicles.size(); i++) {
        mappy[{vehicles[i].x, vehicles[i].y}] = idx++;
    }
    mappy[{employees[0].destX, employees[0].destY}] = idx;

    auto solution = solveALNS(employees, vehicles, meta);

    double totalCost = 0;
    double globalDist = 0;
    double globalTime = 0;
    double globalMoneyCost = 0;

    for (const auto &r : solution) {
        std::cout << "Vehicle " << vehicles[r.vehicleId].originalId << ": ";

        if (r.seq.empty()) {
            std::cout << "Unused\n";
            continue;
        }

        double d = 0, time = 0;
        printRouteTrace(r, vehicles[r.vehicleId], employees, d, time, meta);

        globalDist += d;
        globalTime += time;
        globalMoneyCost += d * vehicles[r.vehicleId].costPerKm;

        std::cout << " [Debug: Dist=" << d << " km, CostPerKm=" << vehicles[r.vehicleId].costPerKm
                  << ", RouteCost=" << d * vehicles[r.vehicleId].costPerKm << "]";

        std::cout << "\n";
        totalCost += routeCost(r, vehicles[r.vehicleId], employees, meta);
    }
    std::cout << "Total Cost (Optimization Score) in ALNS: " << totalCost << "\n";

    std::cout << "------------------------------------------------\n";
    std::cout << "Total Distance (km): " << globalDist << "\n";
    std::cout << "Total Travel Cost (Money): " << globalMoneyCost << "\n";
    std::cout << "Total Time (min): " << globalTime << "\n";

    double objective = globalMoneyCost * meta.objectiveCostWeight + globalTime * meta.objectiveTimeWeight;
    std::cout << "Custom Objective (w1*Money + w2*Time): " << objective << "\n";

    generateOutputFiles(solution, vehicles, employees, meta, argv[1]);
    auto stop = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    std::cout << "Time taken: " << duration.count() << " ms" << std::endl;
}
