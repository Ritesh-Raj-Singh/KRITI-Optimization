#pragma once

#ifndef MATRIX_H
#define MATRIX_H

#include <string>
#include <vector>

// Global variables for dimensions
extern int N;
extern int V;

// Matrix storage
extern std::vector<std::vector<double>> matrix;

// API
void loadMatrix(const std::string &filename, int size);
int convert(const std::string &a);
double getDistanceFromMatrix(const std::string &a, const std::string &b);
int getTravelTimeFromMatrix(const std::string &a, const std::string &b, double speed_kmh);

#endif
