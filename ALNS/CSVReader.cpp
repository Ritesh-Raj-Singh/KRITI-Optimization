#include "CSVReader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

// Helper to parse HH:MM to minutes
static double parseTime(const std::string& tStr) {
    size_t colon = tStr.find(':');
    if (colon == std::string::npos) return 0.0;
    try {
        int h = std::stoi(tStr.substr(0, colon));
        int m = std::stoi(tStr.substr(colon + 1));
        return h * 60.0 + m;
    } catch (...) {
        return 0.0;
    }
}

// Helper to trim check
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::vector<Employee> readEmployees(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error opening file: " << path << std::endl;
        return {};
    }
    std::string line;
    std::getline(f, line); // Skip header

    std::vector<Employee> data;
    int dataIdx = 0;

    // Headers: User identifier,Priority,PickX,PickY,DestX,DestY,Start,End,VehPref,SharePref
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> row;
        while (std::getline(ss, token, ',')) {
            row.push_back(trim(token));
        }
        if (row.size() < 10) continue;

        Employee e;
        e.id = dataIdx++;
        e.originalId = row[0];
        try {
            e.priority = std::stoi(row[1]);
            e.x = std::stod(row[2]);
            e.y = std::stod(row[3]);
            e.destX = std::stod(row[4]);
            e.destY = std::stod(row[5]);
            e.ready = parseTime(row[6]);
            e.due = parseTime(row[7]);
        } catch (...) {
            std::cerr << "Parse error in line: " << line << std::endl;
            continue;
        }
        
        // Vehicle pref
        std::string vp = row[8];
        if (vp == "premium") e.wantsPremium = true;
        else e.wantsPremium = false;

        // Share pref
        std::string sp = row[9];
        if (sp == "single") e.sharePref = 1;
        else if (sp == "double") e.sharePref = 2;
        else if (sp == "triple") e.sharePref = 3;
        else e.sharePref = 3; // Default?

        data.push_back(e);
    }
    return data;
}

std::vector<Vehicle> readVehicles(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error opening file: " << path << std::endl;
        return {};
    }
    std::string line;
    std::getline(f, line); // Skip header

    std::vector<Vehicle> data;
    int dataIdx = 0;

    // Headers: ID,Fuel,Mode,Cap,Cost,Speed,X,Y,Avail,Cat
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> row;
        while (std::getline(ss, token, ',')) {
            row.push_back(trim(token));
        }
        if (row.size() < 10) continue;

        Vehicle v;
        v.id = dataIdx++;
        v.originalId = row[0];
        v.fuelType = row[1];
        v.mode = row[2];
        try {
            v.seatCap = std::stoi(row[3]);
            v.costPerKm = std::stod(row[4]);
            v.speed = std::stod(row[5]);
            v.x = std::stod(row[6]);
            v.y = std::stod(row[7]);
            v.startTime = parseTime(row[8]);
        } catch (...) {
            std::cerr << "Parse error in line: " << line << std::endl;
            continue;
        }

        v.endTime = 24.0 * 60.0; // Default end of day

        std::string cat = row[9];
        v.premium = (cat == "premium");

        data.push_back(v);
    }
    return data;
}
