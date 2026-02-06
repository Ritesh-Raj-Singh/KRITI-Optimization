#pragma once
#include <string>

struct Employee {
    int id; // Internal index
    std::string originalId;
    int priority;
    double x, y;
    double destX, destY;
    double ready, due; // In minutes from midnight
    bool wantsPremium;
    int sharePref; // 1=Single, 2=Double, 3=Triple
};
