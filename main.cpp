#include "crow_all.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <iomanip>

using json = nlohmann::json;

std::mutex log_mutex;

int N, V;
// 0 -> N-1 employees               E0X = X-1;
// N -> V+N-1 vehicles              V0Y = N+Y-1;
// V+N -> dropoff                   N+V;
std::vector<std::vector<int>> matrix;

std::unordered_map<std::string, double> veh;
double cost_weight;
double time_weight;

// cost Calculation
int timeStringToMin(std::string timeStr)
{
    int h = std::stoi(timeStr.substr(0, 2));
    int m = std::stoi(timeStr.substr(3, 2));
    return h * 60 + m;
}

int convert(std::string a)
{
    if (a[0] == 'E')
    {
        std::string t = a.substr(1, a.size() - 1);
        int idx = std::stoi(t);
        idx--;
        return idx;
    }
    else if (a[0] == 'V')
    {
        std::string t = a.substr(1, a.size() - 1);
        int idx = std::stoi(t);
        idx--;
        return V + idx;
    }
    return N + V;
}

double cost(std::string &veh_o, int &unserved_requests)
{
    std::ifstream f1(veh_o);
    if (!f1.is_open())
    {
        std::cerr << "Error opening file\n";
        return -1;
    }

    std::string s;
    std::getline(f1, s);

    double passenger_time = 0;
    double total_cost = 0;

    std::set<int> unserved;
    for (int i = 0; i < N; i++)
        unserved.insert(i);

    std::unordered_map<int,
                       std::vector<std::pair<std::pair<int, int>, int>>>
        mp;

    while (std::getline(f1, s))
    {
        std::stringstream ss(s);
        std::string vehicle, t, employee;
        std::string pickup_time, dropoff_time;

        std::getline(ss, vehicle, ',');
        std::getline(ss, t, ',');
        std::getline(ss, employee, ',');
        std::getline(ss, pickup_time, ',');
        std::getline(ss, dropoff_time, ',');

        int veh_id = convert(vehicle);
        int emp_id = convert(employee);

        int p_time = timeStringToMin(pickup_time);
        int d_time = timeStringToMin(dropoff_time);

        passenger_time += std::max(0, d_time - p_time);
        unserved.erase(emp_id);

        mp[veh_id].push_back({{d_time, p_time}, emp_id});
    }

    for (auto &it : mp)
    {
        int veh_id = it.first;
        double distance = 0;

        auto &trip = it.second;
        std::sort(trip.begin(), trip.end());

        int drop = trip[0].first.first;
        int curr = trip[0].second;

        distance += matrix[veh_id][curr];

        for (int i = 1; i < (int)trip.size(); i++)
        {
            if (trip[i].first.first == drop)
            {
                distance += matrix[trip[i].second][curr];
            }
            else
            {
                distance += matrix[curr][N + V];
                distance += matrix[N + V][trip[i].second];
                drop = trip[i].first.first;
            }
            curr = trip[i].second;
        }

        distance += matrix[curr][N + V];

        total_cost += veh[it.first] * distance;
    }

    unserved_requests = (int)unserved.size();
    return total_cost * cost_weight + passenger_time * time_weight;
}

/* ===================== FILE HELPERS ===================== */
void saveFile(const std::string &filename, const std::string &content)
{
    std::ofstream out(filename);
    out << content;
    out.close();
}

std::string readFile(const std::string &path)
{
    std::ifstream t(path);
    if (!t.is_open())
        return "";
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

std::vector<std::string> splitLine(const std::string &line)
{
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, ','))
        out.push_back(token);
    return out;
}

/* ===================== STEP 1: MATRIX GENERATION ===================== */
// Generates 'matrix.txt' and returns a status/log JSON
json generate_matrix_file(const std::string &empData, const std::string &vehData)
{
    json result;
    std::vector<std::pair<double, double>> coords;

    // 1. Parse Employees (Pickups)
    std::stringstream empSS(empData);
    std::string line;
    std::getline(empSS, line);
    while (std::getline(empSS, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto c = splitLine(line);
        if (c.size() < 4)
            continue;
        // Assuming: ID, Prio, PickLat(2), PickLng(3) ...
        coords.push_back({std::stod(c[2]), std::stod(c[3])});
    }

    // 2. Parse Vehicles (Current Locations)
    std::stringstream vehSS(vehData);
    std::getline(vehSS, line);
    while (std::getline(vehSS, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto c = splitLine(line);
        if (c.size() < 8)
            continue;
        // Assuming: ID... CurLat(6), CurLng(7)
        coords.push_back({std::stod(c[6]), std::stod(c[7])});
    }

    // 3. Construct URL
    if (coords.empty())
    {
        result["status"] = "error";
        result["message"] = "No coordinates found in CSVs";
        return result;
    }

    std::ostringstream url_coords;
    for (size_t i = 0; i < coords.size(); i++)
    {
        url_coords << coords[i].second << "," << coords[i].first; // Lng, Lat
        if (i + 1 < coords.size())
            url_coords << ";";
    }

    std::string url = "http://router.project-osrm.org/table/v1/driving/" + url_coords.str() + "?annotations=distance";

    // 4. Call OSRM
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "[Backend] Fetching Matrix..." << std::endl;
    }

    int ret = std::system(("curl -s \"" + url + "\" -o osrm_raw.json").c_str());

    if (ret != 0)
    {
        result["status"] = "error";
        result["message"] = "Curl failed";
        return result;
    }

    // 5. Parse JSON and Write Simple Text File
    std::ifstream jsonFile("osrm_raw.json");
    json j;
    try
    {
        jsonFile >> j;
        if (!j.contains("distances"))
            throw std::runtime_error("No distances in OSRM response");

        std::ofstream txtOut("matrix.txt");
        auto matrix = j["distances"];

        // Write dimensions first
        txtOut << matrix.size() << " " << matrix[0].size() << "\n";

        for (const auto &row : matrix)
        {
            for (const auto &val : row)
            {
                txtOut << (val.get<double>() / 1000.0) << " "; // Convert to KM
            }
            txtOut << "\n";
        }
        txtOut.close();

        result["status"] = "success";
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            std::cout << "[Backend] Matrix saved to 'matrix.txt'" << std::endl;
        }
    }
    catch (std::exception &e)
    {
        result["status"] = "error";
        result["message"] = e.what();
    }
    return result;
}

/* ===================== STEP 2: SOLVER RUNNER ===================== */
struct SolverResult
{
    std::string status;
    std::string logs;
    std::string output_vehicle;
    std::string output_employee;
};

SolverResult run_solver(std::string folder, std::string execName, std::string args)
{
    SolverResult res;

    // Command: cd [folder] && ./[exec] [args] > log.txt
    std::string logFile = "run_log.txt";
    std::string command = "cd " + folder + " && ./" + execName + " " + args + " > " + logFile;

    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "[Backend] Starting " << folder << "..." << std::endl;
    }

    int returnCode = std::system(command.c_str());

    res.logs = readFile(folder + "/" + logFile);
    res.output_vehicle = readFile(folder + "/output_vehicle.csv");
    res.output_employee = readFile(folder + "/output_employees.csv");

    if (returnCode != 0)
        res.status = "error";
    else if (res.output_vehicle.empty())
        res.status = "no_output";
    else
        res.status = "success";

    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "[Backend] Finished " << folder << "." << std::endl;
    }
    return res;
}

/* ===================== MAIN SERVER ===================== */
int main()
{
    crow::SimpleApp app;
    app.loglevel(crow::LogLevel::Warning);

    CROW_ROUTE(app, "/upload").methods(crow::HTTPMethod::POST)([](const crow::request &req)
                                                               {
        crow::multipart::message msg(req);

        std::string empData, vehData, metaData, baseData;

        // 1. Safe File Extraction (using .find, NOT .at)
        auto emp_it = msg.part_map.find("file_employees.csv");
        if (emp_it != msg.part_map.end()) empData = emp_it->second.body;

        auto veh_it = msg.part_map.find("file_vehicles.csv");
        if (veh_it != msg.part_map.end()) vehData = veh_it->second.body;

        auto meta_it = msg.part_map.find("file_meta.csv");
        if (meta_it != msg.part_map.end()) metaData = meta_it->second.body;

        auto base_it = msg.part_map.find("file_baseline.csv");
        if (base_it != msg.part_map.end()) baseData = base_it->second.body;

        // Check if ANY required file is missing
        if (empData.empty() || vehData.empty() || metaData.empty() || baseData.empty()) {
            return crow::response(400, "Missing one of the 4 CSVs (employees, vehicles, meta, or baseline).");
        }

        // 2. Save Raw Inputs to Root
        saveFile("vehicles.csv", vehData);
        saveFile("employees.csv", empData);
        saveFile("metadata.csv", metaData);
        saveFile("baseline.csv", baseData);

        // 3. GENERATE MATRIX (Sequential)
        // We use the vehicles and employees data to query OSRM
        json matrix_status = generate_matrix_file(empData, vehData);
        
        if (matrix_status["status"] == "error") {
            return crow::response(500, "Matrix Calculation Failed: " + matrix_status["message"].get<std::string>());
        }

        // 4. RUN ALGORITHMS (Parallel)
        // ARGUMENT ORDER: vehicles -> employees -> metadata -> baseline -> matrix
        std::string args = "../vehicles.csv ../employees.csv ../metadata.csv ../baseline.csv ../matrix.txt";
        
        SolverResult alns_res, bac_res;
        
        // Launch threads
        std::thread t1([&](){ alns_res = run_solver("ALNS", "main_ALNS", args); });
        std::thread t2([&](){ bac_res = run_solver("Branch-And-Cut", "main_BAC", args); });

        // Wait for both
        t1.join();
        t2.join();

        // 5. Build Response
        json response;
        response["status"] = "finished";
        response["matrix_info"] = matrix_status;
        
        response["results"]["ALNS"] = {
            {"status", alns_res.status},
            {"logs", alns_res.logs},
            {"csv_vehicle", alns_res.output_vehicle},
            {"csv_employee", alns_res.output_employee}
        };

        response["results"]["BAC"] = {
            {"status", bac_res.status},
            {"logs", bac_res.logs},
            {"csv_vehicle", bac_res.output_vehicle},
            {"csv_employee", bac_res.output_employee}
        };

        crow::response res(200, response.dump());
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Content-Type", "application/json");
        return res; });

    std::cout << "Server running on port 18080..." << std::endl;
    app.port(18080).multithreaded().run();
}