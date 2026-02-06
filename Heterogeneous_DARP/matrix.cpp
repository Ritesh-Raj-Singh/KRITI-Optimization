#include "matrix.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <vector>

// DEFINITIONS
int N = 0;
int V = 0;
std::vector<std::vector<double>> matrix;

void loadMatrix(const std::string &filename, int size)
{
    // Initialize matrix with 0.0
    matrix.assign(size, std::vector<double>(size, 0.0));

    std::ifstream fin(filename);
    if (!fin.is_open())
    {
        std::cerr << "Error: Cannot open matrix file: " << filename << "\n";
        std::exit(1);
    }

    int count = 0;
    double val;
    
    // Read exactly size * size elements
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            if (fin >> val) {
                matrix[i][j] = val;
                count++;
            } else {
                std::cerr << "Error: Matrix file is too small or malformed.\n";
                std::cerr << "Expected " << (size * size) << " elements (for " 
                          << size << "x" << size << " matrix), but found only " << count << ".\n";
                std::cerr << "Check: N=" << N << ", V=" << V << ", +1 Drop. Total Rows=" << size << "\n";
                std::exit(1);
            }
        }
    }
    
    std::cout << "Matrix loaded successfully (" << size << "x" << size << ").\n";
}

int convert(const std::string &a)
{
    // Logic:
    // 0 to N-1: Employees (E1, E2...)
    // N to N+V-1: Vehicles (V1, V2...)
    // N+V: Office (Any other string, e.g., "OFFICE", "DROP")

    if (a.empty()) return N + V; // Safety

    if (a[0] == 'E' || a[0] == 'e') {
        try {
            return std::stoi(a.substr(1)) - 1;
        } catch (...) { return N + V; }
    }

    if (a[0] == 'V' || a[0] == 'v') {
        try {
            return N + std::stoi(a.substr(1)) - 1;
        } catch (...) { return N + V; }
    }

    // Default to Drop Location (Office)
    // This handles "OFFICE", "DROP", or any non-E/non-V string
    return N + V;
}

double getDistanceFromMatrix(const std::string &a, const std::string &b)
{
    int idx_a = convert(a);
    int idx_b = convert(b);
    return matrix[idx_a][idx_b];
}

int getTravelTimeFromMatrix(const std::string &a, const std::string &b, double speed_kmh)
{
    double d = getDistanceFromMatrix(a, b);
    if (speed_kmh <= 0) return 999999;
    // Simple logic: Time = (Dist / Speed) * 60 minutes
    // Adding a small epsilon check for 0 distance
    return (d < 0.005) ? 0 : (int)std::ceil((d / speed_kmh) * 60.0);
}
