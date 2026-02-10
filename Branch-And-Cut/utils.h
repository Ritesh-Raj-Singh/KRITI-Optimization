#ifndef UTILS_H
#define UTILS_H

#include "structures.h"
#include "matrix.h"
#include "globals.h"
#include <string>
#include <vector>
#include <map>

double getDistance(Coords a, Coords b);
int getTravelTime(Coords a, Coords b, double speed_kmh);
int timeStringToMin(std::string timeStr);
std::string minToTimeStr(int minutes);

Request createRequest(
    int id,
    std::string emp_id,
    int priority,
    double p_lat, double p_lng,
    double d_lat, double d_lng,
    std::string t_early, std::string t_late,
    std::string v_pref, std::string s_pref,
    const std::map<int, int> &priority_delays);

// struct InitialTrip
// {
//     std::string vehicle_id;
//     std::string employee_id;
// };
// std::vector<InitialTrip> loadInitialSolutionCSV(const std::string &filename);

#endif