#include "utils.h"
#include <cmath>
#include <string>
#include <iomanip>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int timeStringToMin(std::string timeStr)
{
    int h = std::stoi(timeStr.substr(0, 2));
    int m = std::stoi(timeStr.substr(3, 2));
    return h * 60 + m;
}

double getDistance(Coords a, Coords b)
{
    double R = 6371.0;
    double dLat = (b.lat - a.lat) * M_PI / 180.0;
    double dLon = (b.lng - a.lng) * M_PI / 180.0;
    double lat1 = a.lat * M_PI / 180.0;
    double lat2 = b.lat * M_PI / 180.0;

    double x = sin(dLat / 2) * sin(dLat / 2) +
               sin(dLon / 2) * sin(dLon / 2) * cos(lat1) * cos(lat2);
    double c = 2 * atan2(sqrt(x), sqrt(1 - x));
    return R * c;
}

int getTravelTime(Coords a, Coords b, double speed_kmh)
{
    double dist = getDistance(a, b);

    // FIXED: Handle floating point jitter.
    // If distance is less than 5 meters, assume 0 time.
    if (dist < 0.005)
        return 0;

    return std::ceil((dist / speed_kmh) * 60.0);
}

std::string minToTimeStr(int minutes)
{
    int h = (minutes / 60) % 24;
    int m = minutes % 60;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << h << ":"
        << std::setw(2) << std::setfill('0') << m;
    return oss.str();
}

Request createRequest(
    int id, std::string emp_id, int priority,
    double p_lat, double p_lng, double d_lat, double d_lng,
    std::string t_early, std::string t_late,
    std::string v_pref, std::string s_pref,
    const std::map<int, int> &priority_delays)
{
    Request r;
    r.id = id;
    r.original_id = emp_id;
    r.pickup_loc = {p_lat, p_lng};
    r.drop_loc = {d_lat, d_lng};
    r.earliest_pickup = timeStringToMin(t_early);
    r.latest_drop = timeStringToMin(t_late);

    if (v_pref == "premium")
        r.veh_pref = CATEGORY_PREMIUM;
    else if (v_pref == "normal")
        r.veh_pref = CATEGORY_NORMAL;
    else
        r.veh_pref = CATEGORY_ANY;

    // FIXED: Mapping strings to correct integer logic
    // Single = 0 extra people (Total 1)
    // Double = 1 extra person (Total 2)
    // Triple = 2 extra people (Total 3)
    if (s_pref == "single")
        r.max_shared_with = 0;
    else if (s_pref == "double")
        r.max_shared_with = 1;
    else
        r.max_shared_with = 2;

    // double est_direct_dist = getDistanceFromMatrix(r.original_id, "OFFICE");

    // // Safe check for direct time
    // int direct_time = 0;
    // if (est_direct_dist > 0.005)
    // {
    //     direct_time = std::ceil((est_direct_dist / 30.0) * 60.0);
    // }

    int allowed_delay = priority_delays.at(priority);

    // r.max_ride_time = direct_time + allowed_delay;
    r.latest_drop += allowed_delay;
    r.service_time = 0;

    return r;
}