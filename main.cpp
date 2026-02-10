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
#include <random>
#include <filesystem>

using json = nlohmann::json;
using Matrix = std::vector<std::vector<double>>; // Define Matrix Type

std::mutex log_mutex;

/* ===================== FILE HELPERS ===================== */
void saveFile(const std::string& filename, const std::string& content) {
    std::ofstream out(filename);
    out << content;
    out.close();
}

std::string readFile(const std::string& path) {
    std::ifstream t(path);
    if (!t.is_open()) return "";
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

std::vector<std::string> splitLine(const std::string &line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, ',')) out.push_back(token);
    return out;
}

// stuff for temp dir creation and cleanup

namespace fs = std::filesystem;

std::string generate_uuid()
{
    static std::mt19937_64 rng{std::random_device{}()};
    static std::uniform_int_distribution<uint64_t> dist;

    std::stringstream ss;
    ss << std::hex << dist(rng);
    return ss.str();
}

fs::path create_request_tmp_dir()
{
    fs::path base = "tmp";
    fs::create_directories(base);

    std::string id = "req_" + generate_uuid();
    fs::path reqDir = base / id;

    fs::create_directory(reqDir);
    return reqDir;
}

void cleanup_tmp_dir(const fs::path& dir)
{
    std::error_code ec;
    fs::remove_all(dir, ec);
}

// struct to ensure temp directories are cleaned up after use, even if exceptions occur
struct TempDirGuard {
    fs::path dir;
    ~TempDirGuard() {
        cleanup_tmp_dir(dir);
    }
};


/* ===================== STEP 1: MATRIX GENERATION ===================== */
// Generates 'matrix.txt' AND populates the in-memory 'outMatrix' vector
json generate_matrix_file(const std::string& empData, const std::string& vehData, Matrix& outMatrix, const fs::path& reqDir) {
    json result;
    std::vector<std::pair<double, double>> coords;

    // 1. Parse Employees (Pickups)
    std::pair<double, double> hold;
    std::stringstream empSS(empData);
    std::string line;
    std::getline(empSS, line); 
    while (std::getline(empSS, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto c = splitLine(line);
        if (c.size() < 4) continue;
        coords.push_back({std::stod(c[2]), std::stod(c[3])});
        hold = {std::stod(c[4]), std::stod(c[5])};
    }

    // 2. Parse Vehicles (Current Locations)
    std::stringstream vehSS(vehData);
    std::getline(vehSS, line); 
    while (std::getline(vehSS, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto c = splitLine(line);
        if (c.size() < 8) continue;
        coords.push_back({std::stod(c[6]), std::stod(c[7])});
    }


    // 3. Parse Ofice (final location)

    coords.push_back(hold);


    // std::stringstream offSS(empData);
    // std::getline(empSS, line);
    // std::getline(empSS, line);
    // auto c = splitLine(line);
    // std::cerr << "\n\n\n\n\n\n\n\n\\n";
    // coords.push_back({std::stod(c[4]), std::stod(c[5])}); 
    // std::cerr << c[4] << " " << c[5] << std::endl;

    // 4. Construct URL
    if (coords.empty()) {
        result["status"] = "error";
        result["message"] = "No coordinates found in CSVs";
        return result;
    }

    std::ostringstream url_coords;
    for (size_t i = 0; i < coords.size(); i++) {
        url_coords << coords[i].second << "," << coords[i].first; // Lng, Lat
        if (i + 1 < coords.size()) url_coords << ";";
    }

    std::string url = "http://router.project-osrm.org/table/v1/driving/" + url_coords.str() + "?annotations=distance";
    
    // 4. Call OSRM
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "[Backend] Fetching Matrix..." << std::endl;
    }
    
    int ret = std::system(("curl -s \"" + url + "\" -o " + (reqDir / "osrm_raw.json").string()).c_str());

    if (ret != 0) {
        result["status"] = "error";
        result["message"] = "Curl failed";
        return result;
    }

    // 5. Parse JSON, Write File, AND Fill Vector
    std::ifstream jsonFile((reqDir / "osrm_raw.json").string());
    json j;
    try {
        jsonFile >> j;
        if (!j.contains("distances")) throw std::runtime_error("No distances in OSRM response");

        std::ofstream txtOut((reqDir / "matrix.txt").string());
        auto matrixJson = j["distances"];
        
        // Clear internal vector to be safe
        outMatrix.clear();

        // Write dimensions to file
        // txtOut << matrixJson.size() << " " << matrixJson[0].size() << "\n";

        for (const auto &row : matrixJson) {
            std::vector<double> rowVec;
            for (const auto &val : row) {
                double km = val.get<double>() / 1000.0; // Convert to KM
                
                // Write to File
                txtOut << km << " ";
                
                // Add to In-Memory Vector
                rowVec.push_back(km);
            }
            txtOut << "\n";
            outMatrix.push_back(rowVec);
        }
        txtOut.close();
        
        result["status"] = "success";
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            std::cout << "[Backend] Matrix processed. Size: " << outMatrix.size() << "x" 
                      << (outMatrix.empty() ? 0 : outMatrix[0].size()) << std::endl;
        }

    } catch (std::exception& e) {
        result["status"] = "error";
        result["message"] = e.what();
    }
    return result;
}

/* ===================== STEP 2: SOLVER RUNNER ===================== */
struct SolverResult {
    std::string status;
    std::string logs;
    std::string output_vehicle;
    std::string output_employee;
};

SolverResult run_solver(std::string folder, std::string execName, std::string args, fs::path reqDir) {
    SolverResult res;
    
    std::string logFile = (reqDir / folder / "run_log.txt").string();
    std::string command = "cd " + folder + " && ./" + execName + " " + args + " > ../" + logFile;

    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "[Backend] Starting " << folder << "..." << std::endl;
    }

    int returnCode = std::system(command.c_str());

    res.logs = readFile(logFile);
    res.output_vehicle = readFile(reqDir / folder / "output_vehicle.csv");
    res.output_employee = readFile(reqDir / folder / "output_employees.csv");
    
    if (returnCode != 0) res.status = "error";
    else if (res.output_vehicle.empty()) res.status = "no_output";
    else res.status = "success";

    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "[Backend] Finished " << folder << "." << std::endl;
    }
    return res;
}

/* ===================== MAIN SERVER ===================== */
int main() {
    crow::SimpleApp app;
    app.loglevel(crow::LogLevel::Warning);

    CROW_ROUTE(app, "/upload").methods(crow::HTTPMethod::POST)([](const crow::request& req) {
        crow::multipart::message msg(req);

        std::string empData, vehData, metaData, baseData;

        // 1. Safe File Extraction
        auto emp_it = msg.part_map.find("employees");
        if (emp_it != msg.part_map.end()) empData = emp_it->second.body;

        auto veh_it = msg.part_map.find("vehicles");
        if (veh_it != msg.part_map.end()) vehData = veh_it->second.body;

        auto meta_it = msg.part_map.find("metadata");
        if (meta_it != msg.part_map.end()) metaData = meta_it->second.body;

        // auto base_it = msg.part_map.find("baseline");
        // if (base_it != msg.part_map.end()) baseData = base_it->second.body;
        
        
        if (empData.empty() || vehData.empty() || metaData.empty()) {
            return crow::response(400, "Missing one of the 3 CSVs.");
        }

        fs::path reqDir = create_request_tmp_dir();
        TempDirGuard guard{reqDir}; // Uncomment if you want automatic cleanup after request handling
        std::cerr << "Created temp directory: " << reqDir << std::endl;
        fs::path base = reqDir / "ALNS";
        fs::create_directories(base);
        base = reqDir / "Clustering-Routing-DP-Solver";
        fs::create_directories(base);
        base = reqDir / "Heterogeneous_DARP";
        fs::create_directories(base);
        // base = reqDir / "Variable_Neighbourhood_Search-KRITI";
        // fs::create_directories(base);


        // 2. Save Raw Inputs
        saveFile((reqDir / "employees.csv").string(), empData);
        saveFile((reqDir / "vehicles.csv").string(), vehData);
        saveFile((reqDir / "metadata.csv").string(), metaData);
        // saveFile((reqDir / "baseline.csv").string(), baseData);

        // 3. GENERATE MATRIX (File + Vector)
        Matrix internal_matrix; // This will hold your matrix in C++ memory
        const std::string matrixPath = (reqDir / "matrix.txt").string();
        json matrix_status = generate_matrix_file(empData, vehData, internal_matrix, reqDir);
    
        if (matrix_status["status"] == "error") {
            return crow::response(500, "Matrix Calculation Failed: " + matrix_status["message"].get<std::string>());
        }

        // 4. RUN ALGORITHMS (Parallel)
        // ARGUMENT ORDER: vehicles -> employees -> metadata -> matrix
        std::string args = "../"+reqDir.string();
        
        SolverResult alns_res, bac_res, crds_res, hd_res, vnsk_res;
       

        std::thread t1([&](){ alns_res = run_solver("ALNS", "main_ALNS", args, reqDir); });
        std::thread t2([&](){ bac_res = run_solver("Branch-And-Cut", "main_BAC", args, reqDir); });
        std::thread t3([&](){ crds_res = run_solver("Clustering-Routing-DP-Solver", "crdp", args, reqDir); });
        std::thread t4([&](){ hd_res = run_solver("Heterogeneous_DARP", "hetero", args, reqDir); });
        std::thread t5([&](){ vnsk_res = run_solver("Variable_Neighbourhood_Search-KRITI", "main_vns", args, reqDir); });

        t1.join();
        // t2.join();
        t3.join();
        t4.join();
        // t5.join();
        

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

        response["results"]["CRDS"] = {
            {"status", crds_res.status},
            {"logs", crds_res.logs},
            {"csv_vehicle", crds_res.output_vehicle},
            {"csv_employee", crds_res.output_employee}
        };

        response["results"]["HD"] = {
            {"status", hd_res.status},
            {"logs", hd_res.logs},
            {"csv_vehicle", hd_res.output_vehicle},
            {"csv_employee", hd_res.output_employee}
        };

        response["results"]["VNSK"] = {
            {"status", vnsk_res.status},
            {"logs", vnsk_res.logs},
            {"csv_vehicle", vnsk_res.output_vehicle},
            {"csv_employee", vnsk_res.output_employee}
        };

        crow::response res(200, response.dump());
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Content-Type", "application/json");
        return res;
    });

    std::cout << "Server running on port 5555..." << std::endl;
    app.port(5555).multithreaded().run();
}