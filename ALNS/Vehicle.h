#pragma once
#include <string>

struct Vehicle {
    int id; // Internal index
    std::string originalId;
    std::string fuelType;
    std::string mode;
    int seatCap;
    double costPerKm;
    double speed;
    double x, y;
    double startTime; // In minutes from midnight
    double endTime;   // In minutes from midnight (default to end of day)
    bool premium;
};
