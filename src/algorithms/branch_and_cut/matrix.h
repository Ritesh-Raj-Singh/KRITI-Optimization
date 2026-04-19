#pragma once

#ifndef MATRIX_H
#define MATRIX_H

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "globals.h"

extern std::vector<std::vector<double>> matrix;

extern std::unordered_map<std::string, int> g_matrixIndex;
extern int g_officeMatrixIdx;

void registerMatrixId(const std::string &original_id, int col_index);
void registerOfficeIdx(int col_index);

int matrixIdxOf(const std::string &original_id);

void loadMatrix(const std::string &filename, int size);

double getDistanceFromMatrix(const std::string &a, const std::string &b);
int getTravelTimeFromMatrix(const std::string &a, const std::string &b, double speed_kmh);

inline double getDistanceByIndex(int from, int to) { return matrix[from][to]; }

inline int getTravelTimeByIndex(int from, int to, double speed_kmh) {
    double d = matrix[from][to];
    if (speed_kmh <= 0) {
        if (d < 0.005) return 0;
        return 1e7;
    }
    return (d < 0.005) ? 0 : (int)std::ceil((d / speed_kmh) * 60.0);
}

#endif