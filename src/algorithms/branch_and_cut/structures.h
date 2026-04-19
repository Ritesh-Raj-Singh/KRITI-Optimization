#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <string>
#include <vector>

#include "globals.h"

enum VehicleCategory { CATEGORY_NORMAL, CATEGORY_PREMIUM, CATEGORY_ANY };
enum SharingPref { SHARE_SINGLE = 1, SHARE_DOUBLE = 2, SHARE_TRIPLE = 3 };

struct Coords {
    double lat;
    double lng;
};

struct Request {
    int id;
    std::string original_id;
    Coords pickup_loc;
    Coords drop_loc;
    int earliest_pickup;
    int latest_drop;
    int service_time = 0;
    int max_ride_time;
    VehicleCategory veh_pref;
    int max_shared_with;
    int priority;

    bool isVehicleCompatible(VehicleCategory v_cat) const {
        if (veh_pref == CATEGORY_ANY) return true;

        if (veh_pref == CATEGORY_NORMAL) return v_cat == CATEGORY_NORMAL;
        if (veh_pref == CATEGORY_PREMIUM) return v_cat == CATEGORY_PREMIUM;

        return false;
    }
};

struct Vehicle {
    int id;
    std::string original_id;
    VehicleCategory category;
    int max_capacity;
    Coords start_loc;
    int available_from;
    double cost_per_km;
    double avg_speed_kmh;
    int dummy_start_node_id;
    int dummy_end_node_id;
};

struct Node {
    int id;
    enum Type { SUPER_SOURCE, SUPER_SINK, DUMMY_START, DUMMY_END, PICKUP, DELIVERY };
    Type type;
    int request_id;
    int vehicle_id;
    int demand;
    int earliest_time;
    int latest_time;
    int service_duration = 0;

    int matrix_idx = 0;

    std::string getMatrixId(const std::vector<Request> &reqs, const std::vector<Vehicle> &vehs) const {
        if (type == PICKUP) return reqs[request_id].original_id;

        if (type == DELIVERY || type == DUMMY_END || type == SUPER_SINK) return "OFFICE";

        if (type == DUMMY_START) return vehs[vehicle_id].original_id;

        return "";
    }

    inline int getMatrixIndex() const { return matrix_idx; }

    std::string getOriginalRequestId(const std::vector<Request> &reqs) const {
        if ((type == PICKUP || type == DELIVERY) && request_id >= 0 && request_id < (int)reqs.size()) {
            return reqs[request_id].original_id;
        }
        return "";
    }

    std::string getOriginalVehicleId(const std::vector<Vehicle> &vehs) const {
        if ((type == DUMMY_START || type == DUMMY_END) && vehicle_id >= 0 && vehicle_id < (int)vehs.size()) {
            return vehs[vehicle_id].original_id;
        }
        return "";
    }

    Coords getCoords(const std::vector<Request> &reqs, const std::vector<Vehicle> &vehs) const {
        if (type == PICKUP) return reqs[request_id].pickup_loc;

        if (type == DELIVERY) return reqs[request_id].drop_loc;

        if (type == DUMMY_START) return vehs[vehicle_id].start_loc;

        if (type == DUMMY_END) {
            if (!reqs.empty()) {
                return reqs[0].drop_loc;
            }
            return vehs[vehicle_id].start_loc;
        }

        return {0.0, 0.0};
    }
};

#endif