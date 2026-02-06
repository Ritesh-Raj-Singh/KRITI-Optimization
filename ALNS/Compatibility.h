#pragma once
#include "Employee.h"
#include "Vehicle.h"
#include <algorithm>

// Check if Employee e CAN be in Vehicle v based on static properties?
// Note: seatCompatible in the problem context might refer to 'sharePref' vs 'occupancy'.
// But as a static check:
// if e needs a 'single' ride (sharePref=1), and v.seatCap >= 1 (always true).
// BUT "Two share and three share employees can be assigned to lower seats but not vice versa".
// This implies:
// Employee with SharePref=K can accept a ride with MaxOccupancy <= K.
// "Lower seats" -> Fewer people.
// So, e.sharePref is the MAX people they tolerate.
// This is a dynamic check on the route, but strictly, a vehicle MUST have capacity for at least 1 person.
// Static compatibility is trivial unless sharePref > v.seatCap?
// If e tolerates 3 people, but car only holds 2, is that OK? Yes.
// If e only tolerates 1 person, car holds 4. Can we put them there? Yes, if we don't pick up others.
// So seatCompatibility is just: Can the car physically hold the person?
inline bool seatCompatible(const Employee& e, const Vehicle& v){
    return v.seatCap >= 1;
}

// "Non premium selected employees can be assigned" [to Premium]
// "Premium selected employees" [can they be assigned to Non-Premium?]
// Usually "Premium Preference" means "I want Premium".
// If I want Premium, I shouldn't get Normal.
// If I don't want Premium, I can get anything? Or I prefer Normal but accept Premium?
// Problem: "Non premium selected employees can be assigned [to premium]...".
// Implication: "Premium selected employees" CANNOT be assigned to Non-Premium.
inline bool premiumCompatible(const Employee& e, const Vehicle& v){
    if (e.wantsPremium && !v.premium) return false;
    return true; // Non-premium user can go in Premium or Non-Premium
}

// ------------------ Hard Lateness Limits (in minutes) ------------------
// You can adjust these values to change the max allowed lateness for each priority.
static const double PRIORITY_1_MAX_LATE = 5.0;
static const double PRIORITY_2_MAX_LATE = 10.0;
static const double PRIORITY_3_MAX_LATE = 15.0;
static const double PRIORITY_4_MAX_LATE = 20.0;
static const double PRIORITY_5_MAX_LATE = 30.0;

inline double getMaxLateness(int priority) {
    switch(priority) {
        case 1: return PRIORITY_1_MAX_LATE;
        case 2: return PRIORITY_2_MAX_LATE;
        case 3: return PRIORITY_3_MAX_LATE;
        case 4: return PRIORITY_4_MAX_LATE;
        case 5: return PRIORITY_5_MAX_LATE;
        default: return PRIORITY_5_MAX_LATE; // Default for unknown priority
    }
}
