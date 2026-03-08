#pragma once
#define PI 3.14159265358979323846
#include <cmath>
#include<map>
#include"mapper.h"



inline double distKm(double x1, double y1, double x2, double y2) {
    int i = mappy[{x1,y1}];
    int j= mappy[{x2,y2}];
    return path_len[i][j];

}