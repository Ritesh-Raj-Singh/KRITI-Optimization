#ifndef UTILS_H
#define UTILS_H

#include <map>
#include <string>
#include <vector>

#include "globals.h"
#include "matrix.h"
#include "structures.h"

double getDistance(Coords a, Coords b);
int getTravelTime(Coords a, Coords b, double speed_kmh);
int timeStringToMin(std::string timeStr);
std::string minToTimeStr(int minutes);

Request createRequest(int id, std::string emp_id, int priority, double p_lat, double p_lng, double d_lat, double d_lng,
                      std::string t_early, std::string t_late, std::string v_pref, std::string s_pref,
                      const std::map<int, int> &priority_delays);

#endif