#include "CSVReader.h"
#include "ALNS.h"
#include "CostFunction.h"
#include <iostream>
#include <fstream>      
#include "Feasibility.h" 

#include "Distance.h"
#include"mapper.h"
#include <iomanip>
#include<chrono>
std::map<std::pair<double, double>, int> mappy;
double path_len[251][251];


bool checkBatchFits(const std::vector<int> &batch, int nextId, const Vehicle &v, const std::vector<Employee> &emp, double currentT, double currentX, double currentY,const Metadata& meta)
{
    
    if (batch.size() + 1 > v.seatCap)
        return false;

    
    const auto &nextE = emp[nextId];
    int newSize = batch.size() + 1;
    if (nextE.sharePref < newSize)
        return false;
    for (int pid : batch)
        if (emp[pid].sharePref < newSize)
            return false;


    double dKm = distKm(currentX, currentY, nextE.x, nextE.y);
    double travelMin = (dKm / v.speed) * 60.0;
    double arrival = currentT + travelMin;
    double startService = std::max(arrival, nextE.ready);
    double depart = startService;

    double dDestKm = distKm(nextE.x, nextE.y, nextE.destX, nextE.destY);
    double timeToDest = (dDestKm / v.speed) * 60.0;
    double arrivalAtDest = depart + timeToDest;

    if (arrivalAtDest > nextE.due + getMaxLateness(nextE.priority,meta))
        return false;


    for (int bid : batch)
    {
        if (arrivalAtDest > emp[bid].due + getMaxLateness(emp[bid].priority,meta))
            return false;
    }

 
    if (arrivalAtDest > v.endTime)
        return false;

    return true;
}

void printRouteTrace(const Route &r, const Vehicle &v, const std::vector<Employee> &emp, double &outDist, double &outDuration, const Metadata& meta)
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
   
        const auto &last = emp[batch.back()];
        double dOff = distKm(currX, currY, last.destX, last.destY);
        double tOff = (dOff / v.speed) * 60.0;
        currT += tOff;
        outDuration += tOff*batch.size();
        totalDist += dOff;

      
        for (int id : batch)
        {
            std::cout << emp[id].originalId << "(drop) -> ";
        }
        currT += 0; 

        currX = last.destX;
        currY = last.destY;
        batch.clear();
    };

    for (int eId : r.seq)
    {
     
        if (!checkBatchFits(batch, eId, v, emp, t, cx, cy,meta))
        {
         
            dropBatch(t, cx, cy);

            
            double dToE = distKm(cx, cy, emp[eId].x, emp[eId].y);
            t += (dToE / v.speed) * 60.0;
            outDuration += batch.size()*((dToE / v.speed) * 60.0);
            totalDist += dToE;
        }
        else
        {
           
            double d = distKm(cx, cy, emp[eId].x, emp[eId].y);
            t += (d / v.speed) * 60.0;
            outDuration += batch.size()*((d / v.speed) * 60.0);
            totalDist += d; 
        }

       
        std::cout << emp[eId].originalId << "(pickup) -> ";

        if(t<emp[eId].ready){
            outDuration+=(emp[eId].ready-t)*batch.size();
        }
        double startService = std::max(t, emp[eId].ready);
        t = startService;
        cx = emp[eId].x;
        cy = emp[eId].y;
        batch.push_back(eId);
    }

   
    if (!batch.empty())
    {
        dropBatch(t, cx, cy);
    }

    std::cout << "End";

    outDist = totalDist;
    
}


std::string formatTime(double mins)
{
    int h = (int)(mins / 60.0);
    int m = (int)mins % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << h << ":"
        << std::setw(2) << std::setfill('0') << m;
    return oss.str();
}



void generateOutputFiles(const std::vector<Route> &solution, const std::vector<Vehicle> &vehicles, const std::vector<Employee> &emp,const Metadata& meta, std::string dir)
{
    std::string vPath = dir + "/ALNS/output_vehicle.csv";
    std::string ePath = dir + "/ALNS/output_employees.csv";



    std::ofstream vFile(vPath);
    std::ofstream eFile(ePath);

    std::cout << "Writing outputs to: " << vPath << "\n";

    double totalOpCost = 0.0;
    double totalPenalty = 0.0;

    for (const Route &r : solution) {
        if (r.seq.empty()) continue;
        CostComponents cc = getRouteCostComponents(r, vehicles[r.vehicleId], emp, meta);
        totalOpCost += cc.operationalCost;
        totalPenalty += cc.penaltyCost;
    }

    vFile << std::fixed << std::setprecision(2) << totalOpCost << "," << totalPenalty << "\n";
    //vFile << "vehicle_id,category,employee_id,pickup_time,drop_time\n";
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
            std::string dropTimeStr = formatTime(arrival); 

            currT = arrival; 

    
            for (auto &item : batch)
            {
                int eId = item.first;
                std::string pickTimeStr = item.second;
                std::string origId = emp[eId].originalId;

              
                vFile << v.originalId << "," << cat << "," << origId << "," << pickTimeStr << "," << dropTimeStr << "\n";

             
                eFile << origId << "," << pickTimeStr << "," << dropTimeStr << "\n";
            }

            currX = last.destX;
            currY = last.destY;
            batch.clear();
        };

        for (int eId : r.seq)
        {
            
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

         
            std::vector<int> currentBatchIds;
            for (auto &p : batch)
                currentBatchIds.push_back(p.first);

         

            if (!checkBatchFits(currentBatchIds, eId, v, emp, t, cx, cy,meta))
            {
                
                processBatch(t, cx, cy);

              
                double d = distKm(cx, cy, emp[eId].x, emp[eId].y);
                t += (d / v.speed) * 60.0;
            }
            else
            {
              
                double d = distKm(cx, cy, emp[eId].x, emp[eId].y);
                t += (d / v.speed) * 60.0;
            }

            double startService = std::max(t, emp[eId].ready);
            std::string pickTimeStr = formatTime(startService);

            t = startService;
            cx = emp[eId].x;
            cy = emp[eId].y;

            batch.push_back({eId, pickTimeStr});
        }

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

    if (argc < 2)
    {
        std::cerr << "Usage: ./main_ALNS directory\n";
        return 1;
    }

    auto vehicles = readVehicles(argv[1] + std::string("/vehicles.csv"));
    auto employees = readEmployees(argv[1] + std::string("/employees.csv"));

     if (vehicles.empty()) {
        std::cerr << "Error: No vehicles loaded from " << argv[1] << "/vehicles.csv\n";
    }           

    if (vehicles.empty() || employees.empty())
    {
        std::cerr << "Error: No vehicles or employees loaded.\n";
        return 1;
    }

    Metadata meta;

    if(argc >= 2 )  meta = readMetadata(argv[1] + std::string("/metadata.csv"));
    else {
        std::cerr << "Warning: No metadata provided. Using default values.\n";
        meta.objectiveCostWeight = 1.0;
        meta.objectiveTimeWeight = 1.0;
    }
    readDist(argv[1] + std::string("/matrix.txt"),(int)(employees.size()+vehicles.size())+1);
    int idx=0;
    for(int i=0;i<employees.size();i++){
        mappy[{employees[i].x,employees[i].y}]=idx++;
    }
       for(int i=0;i<vehicles.size();i++){
        mappy[{vehicles[i].x,vehicles[i].y}]=idx++;
    }
    mappy[{employees[0].destX,employees[0].destY}]=idx;

    auto solution = solveALNS(employees, vehicles, meta);

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
        printRouteTrace(r, vehicles[r.vehicleId], employees, d, time,meta);

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

    generateOutputFiles(solution, vehicles, employees,meta, argv[1]);
    auto stop = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    std::cout << "Time taken: " << duration.count() << " ms" << std::endl;
}
    


