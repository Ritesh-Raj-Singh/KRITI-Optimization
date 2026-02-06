#include "CSVReader.h"
#include "ALNS.h"
#include "CostFunction.h"
#include <iostream>
#include <fstream>       // For CSV output
#include "Feasibility.h" // For helper logic replication if needed, or just Distance/Comp
#include "Compatibility.h"
#include "Distance.h"
#include <iomanip>

#include<chrono>

// Helper to check batch validity (Copied from Feasibility logic for visualization)
bool checkBatchFits(const std::vector<int> &batch, int nextId, const Vehicle &v, const std::vector<Employee> &emp, double currentT, double currentX, double currentY)
{
    // 1. Capacity
    if (batch.size() + 1 > v.seatCap)
        return false;

    // 2. Share Pref
    const auto &nextE = emp[nextId];
    int newSize = batch.size() + 1;
    if (nextE.sharePref < newSize)
        return false;
    for (int pid : batch)
        if (emp[pid].sharePref < newSize)
            return false;

    // 3. Time Feasibility (Lookahead)
    double dKm = distKm(currentX, currentY, nextE.x, nextE.y);
    double travelMin = (dKm / v.speed) * 60.0;
    double arrival = currentT + travelMin;
    double startService = std::max(arrival, nextE.ready);
    double depart = startService + 1.0;

    double dDestKm = distKm(nextE.x, nextE.y, nextE.destX, nextE.destY);
    double timeToDest = (dDestKm / v.speed) * 60.0;
    double arrivalAtDest = depart + timeToDest;

    if (arrivalAtDest > nextE.due + getMaxLateness(nextE.priority))
        return false;

    // Check existing batch members constraints if we take this detour
    // Simplified: In this model, everyone drops at the same time at Office.
    // So we just check if the NEW arrival time at office works for everyone.
    // The previous implementation in Feasibility checks this strictly.
    for (int bid : batch)
    {
        if (arrivalAtDest > emp[bid].due + getMaxLateness(emp[bid].priority))
            return false;
    }

    // Check vehicle end time
    // If we pick up E, then go to Office...
    // Arrival at Office = arrivalAtDest
    if (arrivalAtDest > v.endTime)
        return false;

    return true;
}

void printRouteTrace(const Route &r, const Vehicle &v, const std::vector<Employee> &emp, double &outDist, double &outDuration)
{
    outDist = 0;
    outDuration = 0;
    if (r.seq.empty())
        return;

    double t = v.startTime;
    double cx = v.x, cy = v.y;
    double totalDist = 0;
    std::vector<int> batch;

    auto dropBatch = [&](double &currT, double &currX, double &currY)
    {
        if (batch.empty())
            return;
        // Drive to Office (last pickup -> Office)
        const auto &last = emp[batch.back()];
        double dOff = distKm(currX, currY, last.destX, last.destY);
        double tOff = (dOff / v.speed) * 60.0;
        currT += tOff;
        totalDist += dOff; // Track distance

        // Print Drops
        for (int id : batch)
        {
            std::cout << emp[id].originalId << "(drop) -> ";
        }
        currT += 0; // Drop service time removed

        currX = last.destX;
        currY = last.destY;
        batch.clear();
    };

    for (int eId : r.seq)
    {
        // Check if fits
        if (!checkBatchFits(batch, eId, v, emp, t, cx, cy))
        {
            // Drop current batch
            dropBatch(t, cx, cy);

            // Drive from Office to eId
            double dToE = distKm(cx, cy, emp[eId].x, emp[eId].y);
            t += (dToE / v.speed) * 60.0;
            totalDist += dToE; // Track distance
        }
        else
        {
            // Travel from current to eId
            double d = distKm(cx, cy, emp[eId].x, emp[eId].y);
            t += (d / v.speed) * 60.0;
            totalDist += d; // Track distance
        }

        // Pickup eId
        std::cout << emp[eId].originalId << "(pickup) -> ";

        double startService = std::max(t, emp[eId].ready);
        t = startService;
        cx = emp[eId].x;
        cy = emp[eId].y;
        batch.push_back(eId);
    }

    // Final drop
    if (!batch.empty())
    {
        dropBatch(t, cx, cy);
    }

    std::cout << "End";

    outDist = totalDist;
    outDuration = t - v.startTime;
}

// Helper to format minutes to HH:MM
std::string formatTime(double mins)
{
    int h = (int)(mins / 60.0);
    int m = (int)mins % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << h << ":"
        << std::setw(2) << std::setfill('0') << m;
    return oss.str();
}

// Helper to format minutes to HH:MM
// ... (omitted, assuming it stays or I don't touch it since I am targeting the block)

// Remove filesystem include/namespace

// Helper to format minutes to HH:MM
// (Make sure to duplicate if I am replacing the block containing it)
// Actually, I can just replace the include lines and the function body.

void generateOutputFiles(const std::vector<Route> &solution, const std::vector<Vehicle> &vehicles, const std::vector<Employee> &emp)
{
    std::string vPath = "output_vehicle.csv";
    std::string ePath = "output_employees.csv";

    // Force output to current directory
    // We removed the logic that checks for parent directory to avoid confusion with stale files.

    std::ofstream vFile(vPath);
    std::ofstream eFile(ePath);

    std::cout << "Writing outputs to: " << vPath << "\n";

    vFile << "vehicle_id,category,employee_id,pickup_time,drop_time\n";
    eFile << "employee_id,pickup_time,drop_time\n";

    for (const Route &r : solution)
    {
        if (r.seq.empty())
            continue;

        const Vehicle &v = vehicles[r.vehicleId];
        std::string cat = v.premium ? "premium" : "normal";

        double t = v.startTime;
        double cx = v.x, cy = v.y;

        // Structure to hold pending drops: {empId, pickupTime}
        std::vector<std::pair<int, std::string>> batch;

        auto processBatch = [&](double &currT, double &currX, double &currY)
        {
            if (batch.empty())
                return;

            // Drive to Office
            const auto &last = emp[batch.back().first];
            double dOff = distKm(currX, currY, last.destX, last.destY);
            double tOff = (dOff / v.speed) * 60.0;

            double arrival = currT + tOff;
            std::string dropTimeStr = formatTime(arrival); // Drop time = Arrival at office

            currT = arrival; // Drop service (removed)

            // Write records
            for (auto &item : batch)
            {
                int eId = item.first;
                std::string pickTimeStr = item.second;
                std::string origId = emp[eId].originalId;

                // Write to Vehicle CSV
                vFile << v.originalId << "," << cat << "," << origId << "," << pickTimeStr << "," << dropTimeStr << "\n";

                // Write to Employee CSV
                eFile << origId << "," << pickTimeStr << "," << dropTimeStr << "\n";
            }

            currX = last.destX;
            currY = last.destY;
            batch.clear();
        };

        for (int eId : r.seq)
        {
            // Check fits (logic replication)
            bool fits = true;
            if (batch.size() + 1 > v.seatCap)
                fits = false;
            else
            {
                if (emp[eId].sharePref < (int)batch.size() + 1)
                    fits = false;
                for (auto &item : batch)
                {
                    if (emp[item.first].sharePref < (int)batch.size() + 1)
                        fits = false;
                }
            }

            // Replicate Feasibility Logic Split
            // We need separate batch list to check "fits"
            std::vector<int> currentBatchIds;
            for (auto &p : batch)
                currentBatchIds.push_back(p.first);

            // Simplified check: Just utilize the split logic from before
            // If main loop decided to split, we must split.
            // But we don't have the "split" info in Route.
            // We rely on "re-simulation".
            // The simulation in `printRouteTrace` splits if `!checkBatchFits`.
            // We must use EXACTLY that logic.

            if (!checkBatchFits(currentBatchIds, eId, v, emp, t, cx, cy))
            {
                // DROP
                processBatch(t, cx, cy);

                // Drive to E from Office
                double d = distKm(cx, cy, emp[eId].x, emp[eId].y);
                t += (d / v.speed) * 60.0;
            }
            else
            {
                // Drive to E from Current
                double d = distKm(cx, cy, emp[eId].x, emp[eId].y);
                t += (d / v.speed) * 60.0;
            }

            // Pickup
            double startService = std::max(t, emp[eId].ready);
            std::string pickTimeStr = formatTime(startService);

            t = startService;
            cx = emp[eId].x;
            cy = emp[eId].y;

            batch.push_back({eId, pickTimeStr});
        }

        // Final drop
        if (!batch.empty())
        {
            processBatch(t, cx, cy);
        }
    }

    vFile.close();
    eFile.close();
    std::cout << "Generated output_vehicle.csv and output_employees.csv\n";
}

int main(int argc, char **argv)
{

    auto start = std::chrono::high_resolution_clock::now(); 

    if (argc < 3)
    {
        std::cerr << "Usage: ./vrp <vehicle_csv> <employee_csv>\n";
        return 1;
    }

    auto vehicles = readVehicles(argv[1]);
    auto employees = readEmployees(argv[2]);

    if (vehicles.empty() || employees.empty())
    {
        std::cerr << "Error: No vehicles or employees loaded.\n";
        return 1;
    }

    auto solution = solveALNS(employees, vehicles);

    double totalCost = 0;
    double globalDist = 0;
    double globalTime = 0;
    double globalMoneyCost = 0;

    for (const auto &r : solution)
    {
        std::cout << "Vehicle " << vehicles[r.vehicleId].originalId << ": ";

        if (r.seq.empty())
        {
            std::cout << "Unused\n";
            continue;
        }

        double d = 0, time = 0;
        printRouteTrace(r, vehicles[r.vehicleId], employees, d, time);

        globalDist += d;
        globalTime += time;
        globalMoneyCost += d * vehicles[r.vehicleId].costPerKm;

        std::cout << "\n";
        totalCost += routeCost(r, vehicles[r.vehicleId], employees);
    }
    std::cout << "Total Cost (Optimization Score): " << totalCost << "\n";

    std::cout << "------------------------------------------------\n";
    std::cout << "Total Distance (km): " << globalDist << "\n";
    std::cout << "Total Travel Cost (Money): " << globalMoneyCost << "\n";
    std::cout << "Total Time (min): " << globalTime << "\n";

    // 0.7 * Money + 0.3 * Time
    double objective = globalMoneyCost * 0.7 + globalTime * 0.3;
    std::cout << "Custom Objective (0.7*Money + 0.3*Time): " << objective << "\n";

    generateOutputFiles(solution, vehicles, employees);
    auto stop = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    std::cout << "Time taken: " << duration.count() << " ms" << std::endl;
}
    