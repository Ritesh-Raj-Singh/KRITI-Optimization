#pragma once
#include <string>

struct Employee {
    int id;
    std::string originalId;
    int priority;
    double x, y;
    double destX, destY;
    double ready, due;
    std::string vehiclePref;  // "premium", "normal", or "any"
    int sharePref;
};
