#pragma once

#ifndef MATRIX_H
#define MATRIX_H

#include <vector>
#include <string>
#include <cmath>
#include "globals.h"

// matrix storage
extern std::vector<std::vector<double>> matrix;

// API
void loadMatrix(const std::string &filename, int size);

// String-based (kept for non-hot-path / debugging use)
int convert(const std::string &a);
double getDistanceFromMatrix(const std::string &a, const std::string &b);
int getTravelTimeFromMatrix(const std::string &a,
                            const std::string &b,
                            double speed_kmh);

// Fast integer-index overloads — no string allocation or map lookup
inline double getDistanceByIndex(int from, int to)
{
    return matrix[from][to];
}

inline int getTravelTimeByIndex(int from, int to, double speed_kmh)
{
    double d = matrix[from][to];
    return (d < 0.005) ? 0 : (int)std::ceil((d / speed_kmh) * 60.0);
}

#endif