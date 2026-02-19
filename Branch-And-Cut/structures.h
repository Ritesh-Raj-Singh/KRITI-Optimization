#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <string>
#include <vector>
#include "globals.h"

enum VehicleCategory
{
    CATEGORY_NORMAL,
    CATEGORY_PREMIUM,
    CATEGORY_ANY
};
enum SharingPref
{
    SHARE_SINGLE = 1,
    SHARE_DOUBLE = 2,
    SHARE_TRIPLE = 3
};

struct Coords
{
    double lat;
    double lng;
};

struct Request
{
    int id;
    std::string original_id;
    Coords pickup_loc;
    Coords drop_loc;
    int earliest_pickup;
    int latest_drop;
    int service_time = 0;
    int max_ride_time;
    VehicleCategory veh_pref;
    int max_shared_with; // 0=Single, 1=Double, 2=Triple
    int priority;

    // UPDATED LOGIC:
    // 1. Premium Req: Must use Premium Vehicle.
    // 2. Normal Req: Can use Normal(Upgrade allowed).
    // 3. Any Req: Can use anything.
    bool isVehicleCompatible(VehicleCategory v_cat) const
    {
        if (veh_pref == CATEGORY_ANY)
            return true;

        if (veh_pref == CATEGORY_NORMAL)
            return v_cat == CATEGORY_NORMAL;
        // return v_cat == CATEGORY_NORMAL;

        // If I asked for Premium, I strictly want Premium
        if (veh_pref == CATEGORY_PREMIUM)
            return v_cat == CATEGORY_PREMIUM;

        return false;
    }
};

struct Vehicle
{
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

struct Node
{
    int id;
    enum Type
    {
        SUPER_SOURCE,
        SUPER_SINK,
        DUMMY_START,
        DUMMY_END,
        PICKUP,
        DELIVERY
    };
    Type type;
    int request_id;
    int vehicle_id;
    int demand;
    int earliest_time;
    int latest_time;
    int service_duration = 0;

    std::string getMatrixId(
        const std::vector<Request> &reqs,
        const std::vector<Vehicle> &vehs) const
    {
        if (type == PICKUP)
            return reqs[request_id].original_id; // e.g. "E12"

        if (type == DELIVERY || type == DUMMY_END || type == SUPER_SINK)
            return "OFFICE";

        if (type == DUMMY_START)
            return vehs[vehicle_id].original_id; // e.g. "V3"

        return "";
    }

    // Fast integer index into the distance matrix — no string allocation or parsing.
    // Mirrors convert():  PICKUP/E{i+1} -> i,  DUMMY_START/V{k+1} -> N+k,  everything else -> N+V
    inline int getMatrixIndex() const
    {
        if (type == PICKUP)
            return request_id; // request_id is 0-based → column index i

        if (type == DUMMY_START)
            return N + vehicle_id; // vehicle_id is 0-based → column index N+k

        // DELIVERY, DUMMY_END, SUPER_SINK, SUPER_SOURCE → OFFICE column
        return N + V;
    }

    std::string getOriginalRequestId(const std::vector<Request> &reqs) const
    {
        if ((type == PICKUP || type == DELIVERY) &&
            request_id >= 0 && request_id < (int)reqs.size())
        {
            return reqs[request_id].original_id;
        }
        return "";
    }

    std::string getOriginalVehicleId(const std::vector<Vehicle> &vehs) const
    {
        if ((type == DUMMY_START || type == DUMMY_END) &&
            vehicle_id >= 0 && vehicle_id < (int)vehs.size())
        {
            return vehs[vehicle_id].original_id;
        }
        return "";
    }

    Coords getCoords(const std::vector<Request> &reqs, const std::vector<Vehicle> &vehs) const
    {
        // if (type == PICKUP)
        //     return reqs[request_id].pickup_loc;
        // if (type == DELIVERY)
        //     return reqs[request_id].drop_loc;
        // if (type == DUMMY_START || type == DUMMY_END)
        //     return vehs[vehicle_id].start_loc;
        // return {0.0, 0.0};

        if (type == PICKUP)
            return reqs[request_id].pickup_loc;

        if (type == DELIVERY)
            return reqs[request_id].drop_loc;

        if (type == DUMMY_START)
            return vehs[vehicle_id].start_loc;

        if (type == DUMMY_END)
        {
            if (!reqs.empty())
            {
                return reqs[0].drop_loc; // End at the office/factory
            }
            return vehs[vehicle_id].start_loc; // Fallback
        }
        // --- CHANGE ENDS HERE ---

        return {0.0, 0.0};
    }
};

#endif