#pragma once

#ifndef MATRIX_H
#define MATRIX_H

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "globals.h"

// -----------------------------------------------------------------------
// Matrix storage
// -----------------------------------------------------------------------
extern std::vector<std::vector<double>> matrix;

// -----------------------------------------------------------------------
// Explicit ID -> matrix column/row index map.
//
// WHY: The old convert() parsed "E5" -> 4, "V3" -> N+2.
// This silently breaks when IDs are non-consecutive (E1,E2,E5,E123...)
// or when the matrix was built in CSV-file order rather than ID order.
// The matrix is only sized N+V+1 rows/cols, so "E123" -> index 122 would
// be a hard out-of-bounds crash.
//
// HOW: main() calls registerMatrixId() for every employee and vehicle
// in the exact order they were written into the matrix file (= CSV load
// order).  GraphBuilder then reads matrixIdxOf() to set Node::matrix_idx
// once at build time.  The hot path (getMatrixIndex()) is a single field
// read - zero cost, always correct.
// -----------------------------------------------------------------------
extern std::unordered_map<std::string, int> g_matrixIndex;
extern int g_officeMatrixIdx;

// Call these from main() immediately after loading vehicles & requests,
// in the same order the matrix rows/cols were generated.
void registerMatrixId(const std::string &original_id, int col_index);
void registerOfficeIdx(int col_index);

// Look up a registered id. Aborts with a clear message if not found.
int matrixIdxOf(const std::string &original_id);

// -----------------------------------------------------------------------
// Load & query
// -----------------------------------------------------------------------
void loadMatrix(const std::string &filename, int size);

// String-based (kept for debugging / printing)
double getDistanceFromMatrix(const std::string &a, const std::string &b);
int getTravelTimeFromMatrix(const std::string &a, const std::string &b, double speed_kmh);

// Fast integer-index overloads used by the solver hot path
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