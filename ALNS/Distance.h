#pragma once
#define PI 3.14159265358979323846
#include <cmath>
#include<map>
#include"mapper.h"



inline double distKm(double x1, double y1, double x2, double y2) {
    // 
    // double R = 6371.0;
    // double dLat = (x2-x1) * PI / 180.0;
    // double dLon = (y2-y1) * PI / 180.0;
    // double lat1 = x1 * PI / 180.0;
    // double lat2 = x2 * PI / 180.0;

    // double x = sin(dLat / 2) * sin(dLat / 2) +
    //            sin(dLon / 2) * sin(dLon / 2) * cos(lat1) * cos(lat2);
    // double c = 2 * atan2(sqrt(x), sqrt(1 - x));
    // return R * c;
    int i = mappy[{x1,y1}];
    int j= mappy[{x2,y2}];
    return path_len[i][j];

}