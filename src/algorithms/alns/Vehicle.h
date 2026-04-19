#pragma once
#include <string>

struct Vehicle {
    int id;
    std::string originalId;
    std::string fuelType;
    std::string mode;
    int seatCap;
    double costPerKm;
    double speed;
    double x, y;
    double startTime;
    double endTime;
    bool premium;
};
