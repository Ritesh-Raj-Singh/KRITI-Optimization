#pragma once
#include <cmath>

inline double dist(double x1,double y1,double x2,double y2){
    double dx=x1-x2, dy=y1-y2;
    return std::sqrt(dx*dx+dy*dy);
}

// Convert degrees to KM (Approx for Bangalore/India latitudes)
inline double distKm(double x1, double y1, double x2, double y2) {
    return dist(x1, y1, x2, y2) * 111.0;
}
