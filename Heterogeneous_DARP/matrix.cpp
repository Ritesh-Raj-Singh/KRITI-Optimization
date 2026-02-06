#include "matrix.h"
#include <fstream>
#include <iostream>
#include <cmath>

// DEFINITIONS (one and only one)
int N=0;
int V=0;
std::vector<std::vector<double>> matrix;

void loadMatrix(const std::string &filename, int size)
{
    matrix.assign(size, std::vector<double>(size));

    std::ifstream fin(filename);
    if (!fin)
    {
        std::cerr << "Cannot open matrix file\n";
        std::exit(1);
    }

    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++)
            fin >> matrix[i][j];
}

int convert(const std::string &a)
{
    if (a[0] == 'E')
        return std::stoi(a.substr(1)) - 1;

    if (a[0] == 'V')
        return N + std::stoi(a.substr(1)) - 1;

    // OFFICE / DROP
    return N + V;
}

double getDistanceFromMatrix(const std::string &a, const std::string &b)
{
    return matrix[convert(a)][convert(b)];
}

int getTravelTimeFromMatrix(const std::string &a,
                            const std::string &b,
                            double speed_kmh)
{
    double d = getDistanceFromMatrix(a, b);
    return (d < 0.005) ? 0 : std::ceil((d / speed_kmh) * 60.0);
}