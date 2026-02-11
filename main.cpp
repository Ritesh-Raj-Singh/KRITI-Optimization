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
using Matrix = std::vector<std::vector<double>>;
namespace fs = std::filesystem;

std::mutex log_mutex;

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
    // Ensure we are working with absolute paths from the start
    fs::path base = fs::absolute("tmp");
    fs::create_directories(base);

    std::string id = "req_" + generate_uuid();
    fs::path reqDir = base / id;

    fs::create_directory(reqDir);
    return reqDir;
}

void cleanup_tmp_dir(const fs::path &dir)
{
    std::error_code ec;
    fs::remove_all(dir, ec);
}

struct TempDirGuard
{
    fs::path dir;
    bool active;
    TempDirGuard(fs::path d, bool a = true) : dir(d), active(a) {}
    ~TempDirGuard()
    {
        if (active)
            cleanup_tmp_dir(dir);
    }
};

/* ===================== STEP 1: MATRIX GENERATION ===================== */
json generate_matrix_file(const std::string &empData, const std::string &vehData, Matrix &outMatrix, const fs::path &reqDir)
{
    json result;
    std::vector<std::pair<double, double>> coords;

    try
    {
        // ... (Parsing logic remains the same) ...
        // 1. Parse Employees
        std::pair<double, double> hold = {0.0, 0.0};
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
            coords.push_back({std::stod(c[2]), std::stod(c[3])});
            if (c.size() >= 6)
                hold = {std::stod(c[4]), std::stod(c[5])};
        }

        // 2. Parse Vehicles
        std::stringstream vehSS(vehData);
        std::getline(vehSS, line);
        while (std::getline(vehSS, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            auto c = splitLine(line);
            if (c.size() < 8)
                continue;
            coords.push_back({std::stod(c[6]), std::stod(c[7])});
        }

        // 3. Add Depot
        coords.push_back(hold);

        if (coords.empty())
        {
            result["status"] = "error";
            result["message"] = "No coordinates found";
            return result;
        }

        std::ostringstream url_coords;
        for (size_t i = 0; i < coords.size(); i++)
        {
            url_coords << coords[i].second << "," << coords[i].first;
            if (i + 1 < coords.size())
                url_coords << ";";
        }

        std::string url = "http://router.project-osrm.org/table/v1/driving/" + url_coords.str() + "?annotations=distance";

        fs::path jsonPath = reqDir / "osrm_raw.json";

        // Use -sS to hide progress bar but show errors
        std::string cmd = "curl -sS \"" + url + "\" -o \"" + jsonPath.string() + "\"";
        int ret = std::system(cmd.c_str());

        if (ret != 0)
            throw std::runtime_error("Curl command failed");

        std::ifstream jsonFile(jsonPath);
        json j;
        jsonFile >> j;

        if (!j.contains("distances"))
            throw std::runtime_error("No distances in OSRM response");

        std::ofstream txtOut(reqDir / "matrix.txt");
        auto matrixJson = j["distances"];

        outMatrix.clear();
        for (const auto &row : matrixJson)
        {
            std::vector<double> rowVec;
            for (const auto &val : row)
            {
                double km = val.get<double>() / 1000.0;
                txtOut << km << " ";
                rowVec.push_back(km);
            }
            txtOut << "\n";
            outMatrix.push_back(rowVec);
        }
        txtOut.close();

        result["status"] = "success";
    }
    catch (std::exception &e)
    {
        result["status"] = "error";
        result["message"] = e.what();
        std::cerr << "Matrix Error: " << e.what() << std::endl;
    }
    return result;
}

/* ===================== STEP 2: SOLVER RUNNER (FIXED) ===================== */
struct SolverResult
{
    std::string status;
    std::string logs;
    std::string output_vehicle;
    std::string output_employee;
};

// Fixed Solver Logic
SolverResult run_solver(std::string folderName, std::string execName, fs::path reqDir)
{
    SolverResult res;

    // 1. Identify paths
    fs::path projectRoot = fs::current_path(); // Where the server is running (assumed root)
    fs::path exePath = projectRoot / folderName / execName;
    fs::path runDir = reqDir / folderName; // The temp folder for this specific algo

    // 2. Ensure the run directory inside temp exists
    fs::create_directories(runDir);

    // 3. Construct Command
    // Logic: cd into [tmp/req_id/ALNS] -> run [Root/ALNS/main_ALNS] -> pass [tmp/req_id] as arg
    // This ensures output files are created inside tmp/req_id/ALNS
    std::string logFile = "run_log.txt"; // Relative to runDir

    std::string command = "cd \"" + runDir.string() + "\" && " +
                          "\"" + exePath.string() + "\" \"" + reqDir.string() + "\" > " + logFile + " 2>&1";

    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "[Backend] Running: " << folderName << std::endl;
        // std::cout << "CMD: " << command << std::endl; // Uncomment for debug
    }

    int returnCode = std::system(command.c_str());

    // 4. Read Results from the TEMP directory
    res.logs = readFile((runDir / logFile).string());
    res.output_vehicle = readFile((runDir / "output_vehicle.csv").string());
    res.output_employee = readFile((runDir / "output_employees.csv").string());

    if (returnCode != 0)
    {
        res.status = "error_code_" + std::to_string(returnCode);
    }
    else if (res.output_vehicle.empty())
    {
        res.status = "no_output";
    }
    else
    {
        res.status = "success";
    }

    return res;
}
/* ===================== MAIN SERVER ===================== */
int main()
{
    crow::SimpleApp app;
    app.loglevel(crow::LogLevel::Warning);

    // Increase payload size limit if you are uploading large CSVs
    // app.payload_max(1024 * 1024 * 10); // 10 MB

    CROW_ROUTE(app, "/upload").methods(crow::HTTPMethod::POST)([](const crow::request &req)
                                                               {
        crow::multipart::message msg(req);

        // --- HELPER TO EXTRACT PARTS SAFELY ---
        auto get_part = [&](const std::string& name) -> std::string {
            auto it = msg.part_map.find(name);
            if (it != msg.part_map.end()) {
                return it->second.body;
            }
            return "";
        };

        // 1. Extract Data
        std::string empData = get_part("employees");
        std::string vehData = get_part("vehicles");
        std::string metaData = get_part("metadata");

        if (empData.empty() || vehData.empty() || metaData.empty()) {
            return crow::response(400, "Missing one of the 3 CSVs (employees, vehicles, metadata).");
        }

        // 2. Setup Temp Directory (Absolute Path)
        fs::path reqDir = create_request_tmp_dir();
        
        // --- TEMP DIR CLEANUP ---
        // Change 'false' to 'true' if you want folders deleted automatically after the request finishes.
        // Keeping it 'false' helps you debug by inspecting the tmp folder.
        TempDirGuard guard{reqDir, false}; 

        std::cout << "[Backend] Request ID: " << reqDir.filename().string() << std::endl;

        // 3. Save Raw Inputs
        saveFile((reqDir / "employees.csv").string(), empData);
        saveFile((reqDir / "vehicles.csv").string(), vehData);
        saveFile((reqDir / "metadata.csv").string(), metaData);

        // 4. Generate Matrix
        Matrix internal_matrix; 
        json matrix_status = generate_matrix_file(empData, vehData, internal_matrix, reqDir);
    
        if (matrix_status["status"] == "error") {
            return crow::response(500, "Matrix Failed: " + matrix_status["message"].get<std::string>());
        }

        // 5. Run Algorithms
        // Note: Ensure your external programs (main_ALNS, etc.) accept the directory path as the first argument!
        SolverResult alns_res, bac_res, crds_res, hd_res, vns_res;

        std::thread t1([&](){ alns_res = run_solver("ALNS", "main_ALNS", reqDir); });
        std::thread t2([&](){ bac_res = run_solver("Branch-And-Cut", "main_BAC", reqDir); });
        std::thread t3([&](){ crds_res = run_solver("Clustering-Routing-DP-Solver", "main_crds", reqDir); });
        std::thread t4([&](){ hd_res = run_solver("Heterogeneous_DARP", "hetero", reqDir); });
        std::thread t5([&](){ vns_res = run_solver("Variable_Neighbourhood_Search", "main_vns", reqDir); });

        t1.join();
        t2.join();
        t3.join();
        t4.join();
        t5.join();
        
        // 6. Build Response
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

        response["results"]["VNS"] = {
            {"status", vns_res.status},
            {"logs", vns_res.logs},
            {"csv_vehicle", vns_res.output_vehicle},
            {"csv_employee", vns_res.output_employee}
        };

        crow::response res(200, response.dump());
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Content-Type", "application/json");
        return res; });
        
    std::cout << "Server running on port 5555..." << std::endl;
    app.port(5555).multithreaded().run();
}