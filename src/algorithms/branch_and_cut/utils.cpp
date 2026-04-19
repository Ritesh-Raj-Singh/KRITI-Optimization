#include "utils.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int timeStringToMin(std::string timeStr) {
    size_t colonPos = timeStr.find(':');
    if (colonPos == std::string::npos) return 0;

    int h = std::stoi(timeStr.substr(0, colonPos));
    int m = std::stoi(timeStr.substr(colonPos + 1));
    return h * 60 + m;
}

double getDistance(Coords a, Coords b) {
    double R = 6371.0;
    double dLat = (b.lat - a.lat) * M_PI / 180.0;
    double dLon = (b.lng - a.lng) * M_PI / 180.0;
    double lat1 = a.lat * M_PI / 180.0;
    double lat2 = b.lat * M_PI / 180.0;

    double x = sin(dLat / 2) * sin(dLat / 2) + sin(dLon / 2) * sin(dLon / 2) * cos(lat1) * cos(lat2);
    double c = 2 * atan2(sqrt(x), sqrt(1 - x));
    return R * c;
}

int getTravelTime(Coords a, Coords b, double speed_kmh) {
    double dist = getDistance(a, b);

    if (dist < 0.005) return 0;

    return std::ceil((dist / speed_kmh) * 60.0);
}

std::string minToTimeStr(int minutes) {
    int h = (minutes / 60);
    int m = minutes % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << h << ":" << std::setw(2) << std::setfill('0') << m;
    return oss.str();
}

Request createRequest(int id, std::string emp_id, int priority, double p_lat, double p_lng, double d_lat, double d_lng,
                      std::string t_early, std::string t_late, std::string v_pref, std::string s_pref,
                      const std::map<int, int> &priority_delays) {
    Request r;
    r.id = id;
    r.original_id = emp_id;
    r.pickup_loc = {p_lat, p_lng};
    r.drop_loc = {d_lat, d_lng};
    r.earliest_pickup = timeStringToMin(t_early);
    r.latest_drop = timeStringToMin(t_late);

    if (v_pref == "premium") {
        r.veh_pref = CATEGORY_PREMIUM;
        std::cout << r.original_id << "given preference premimum\n";
    } else if (v_pref == "normal") {
        std::cout << r.original_id << "given preference normal\n";
        r.veh_pref = CATEGORY_NORMAL;
    } else {
        std::cout << r.original_id << "given preference any\n";
        r.veh_pref = CATEGORY_ANY;
    }

    if (s_pref == "single")
        r.max_shared_with = 0;
    else if (s_pref == "double")
        r.max_shared_with = 1;
    else
        r.max_shared_with = 2;

    int allowed_delay = priority_delays.at(priority);

    r.latest_drop += allowed_delay;
    if (r.latest_drop - r.earliest_pickup < 0) r.latest_drop += 1440;
    r.service_time = 0;

    return r;
}
