#include "CSVReader.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "mapper.h"

static double parseTime(const std::string &tStr) {
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

static std::string trim(const std::string &str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::vector<Employee> readEmployees(const std::string &path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error opening file: " << path << std::endl;
        return {};
    }
    std::string line;
    std::getline(f, line);

    std::vector<Employee> data;
    int dataIdx = 0;

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
            if (e.due < e.ready) e.due += 1440.0;
        } catch (...) {
            std::cerr << "Parse error in line: " << line << std::endl;
            continue;
        }

        std::string vp = row[8];
        e.vehiclePref = vp;  // "premium", "normal", or "any"

        std::string sp = row[9];
        if (sp == "single")
            e.sharePref = 1;
        else if (sp == "double")
            e.sharePref = 2;
        else if (sp == "triple")
            e.sharePref = 3;
        else
            e.sharePref = 3;

        data.push_back(e);
    }
    return data;
}

std::vector<Vehicle> readVehicles(const std::string &path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Error opening file: " << path << std::endl;
        return {};
    }
    std::string line;
    std::getline(f, line);
    std::vector<Vehicle> data;
    int dataIdx = 0;

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
        v.endTime = 24.0 * 60.0 * 2.1;
        std::string cat = row[9];
        v.premium = (cat == "premium");
        data.push_back(v);
    }
    return data;
}

void readDist(const std::string &filename, int MAX_SIZE) {
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return;
    }

    for (int i = 0; i < MAX_SIZE; ++i) {
        for (int j = 0; j < MAX_SIZE; ++j) {
            if (!(file >> path_len[i][j])) {
                std::cerr << "Warning: Reached end of file before filling the matrix." << std::endl;
                return;
            }
        }
    }

    file.close();
    std::cout << "Successfully loaded " << MAX_SIZE << "x" << MAX_SIZE << " matrix." << std::endl;
}

Metadata readMetadata(const std::string &path) {
    Metadata meta;
    meta.objectiveCostWeight = 0.7;
    meta.objectiveTimeWeight = 0.3;
    meta.priority_1_max_delay_min = 5;
    meta.priority_2_max_delay_min = 10;
    meta.priority_3_max_delay_min = 15;
    meta.priority_4_max_delay_min = 20;
    meta.priority_5_max_delay_min = 30;

    std::ifstream f(path);

    if (!f.is_open()) {
        std::cerr << "Error opening file: " << path << std::endl;
        return meta;
    }

    std::string line;
    std::getline(f, line);  // header

    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string key, val;
        if (std::getline(ss, key, ',') && std::getline(ss, val, ',')) {
            key = trim(key);
            val = trim(val);
            try {
                if (key == "objective_cost_weight") {
                    meta.objectiveCostWeight = std::stod(val);
                } else if (key == "objective_time_weight") {
                    meta.objectiveTimeWeight = std::stod(val);
                } else if (key == "priority_1_max_delay_min") {
                    meta.priority_1_max_delay_min = std::stoi(val);
                } else if (key == "priority_2_max_delay_min") {
                    meta.priority_2_max_delay_min = std::stoi(val);
                } else if (key == "priority_3_max_delay_min") {
                    meta.priority_3_max_delay_min = std::stoi(val);
                } else if (key == "priority_4_max_delay_min") {
                    meta.priority_4_max_delay_min = std::stoi(val);
                } else if (key == "priority_5_max_delay_min") {
                    meta.priority_5_max_delay_min = std::stoi(val);
                }
            } catch (...) {
                std::cerr << "Error parsing metadata value for key: " << key << std::endl;
            }
        }
    }
    return meta;
}
