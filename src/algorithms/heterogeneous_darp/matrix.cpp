#include "matrix.h"

#include <cmath>
#include <fstream>
#include <iostream>

// Definitions
std::vector<std::vector<double>> matrix;
std::unordered_map<std::string, int> g_matrixIndex;
int g_officeMatrixIdx = 0;

void registerMatrixId(const std::string &original_id, int col_index) { g_matrixIndex[original_id] = col_index; }

void registerOfficeIdx(int col_index) { g_officeMatrixIdx = col_index; }

int matrixIdxOf(const std::string &original_id) {
    auto it = g_matrixIndex.find(original_id);
    if (it == g_matrixIndex.end()) {
        std::cerr << "[FATAL] matrixIdxOf: unknown id '" << original_id
                  << "'. Did you call registerMatrixId() for every employee and vehicle?\n";
        std::exit(1);
    }
    return it->second;
}

void loadMatrix(const std::string &filename, int size) {
    matrix.assign(size, std::vector<double>(size));

    std::ifstream fin(filename);
    if (!fin) {
        std::cerr << "Cannot open matrix file\n";
        std::exit(1);
    }

    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++) fin >> matrix[i][j];

    std::cout << "Generated Matrix: \n";
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) std::cout << matrix[i][j] << " ";
        std::cout << "\n";
    }
}

// String-based helpers (kept for printing / debugging)
double getDistanceFromMatrix(const std::string &a, const std::string &b) {
    return matrix[matrixIdxOf(a)][matrixIdxOf(b)];
}

int getTravelTimeFromMatrix(const std::string &a, const std::string &b, double speed_kmh) {
    double d = getDistanceFromMatrix(a, b);
    if (speed_kmh <= 0) {
        if (d < 0.005) return 0;
        return 1e7;
    }
    return (d < 0.005) ? 0 : (int)std::ceil((d / speed_kmh) * 60.0);
}