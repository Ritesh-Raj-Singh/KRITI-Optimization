#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

const double INF = 1e15;

double OBJ_COST_WEIGHT = 0.7;
double OBJ_TIME_WEIGHT = 0.3;
int PRIORITY_DELAY[6] = {30, 5, 10, 15, 20, 30};

string trim(const string &str) {
    string s = str;
    s.erase(remove(s.begin(), s.end(), '\r'), s.end());
    s.erase(remove(s.begin(), s.end(), '\n'), s.end());
    size_t first = s.find_first_not_of(" \t");
    if (first == string::npos) return "";
    size_t last = s.find_last_not_of(" \t");
    return s.substr(first, (last - first + 1));
}

int getPriorityDelay(int priority) {
    if (priority >= 1 && priority <= 5) return PRIORITY_DELAY[priority];
    return PRIORITY_DELAY[0];
}

double INFEASIBILITY_PENALTY = 0.0;
double PENALTY_PREMIUM_VEHICLE = 500000.0;
double PENALTY_SHARING_PREFERENCE = 500000.0;
double PENALTY_CAPACITY_EXCEEDED = 500000.0;

enum VehicleCat { NORMAL = 0, PREMIUM = 1 };

int timeToMinutes(const string &t) {
    int hh = stoi(t.substr(0, 2));
    int mm = stoi(t.substr(3, 2));
    return hh * 60 + mm;
}

string minToTime(int m) {
    int hh = (m / 60) % 24;
    int mm = m % 60;
    string h_s = (hh < 10 ? "0" : "") + to_string(hh);
    string m_s = (mm < 10 ? "0" : "") + to_string(mm);
    return h_s + ":" + m_s;
}

enum ShareType { SINGLE = 1, DOUBLE = 2, TRIPLE = 3, ANY = 99 };

struct Person {
    int id;
    string original_id;
    int priority;
    double p_lat, p_lng;
    double d_lat, d_lng;
    int early_pickup;
    int late_drop;
    VehicleCat pref_vehicle;
    int load;
    ShareType max_sharing;
};

struct Driver {
    int id;
    string original_id;
    int capacity;
    double cost_per_km;
    double speed_kmph;
    double start_lat, start_lng;
    int start_time;
    VehicleCat category;
    string type_str;
};

class Chromosome {
   public:
    vector<int> giantTour;
    double fitness;
    int numVehiclesUsed;

    struct Trip {
        int vehicleIdx;
        vector<int> customers;
        double startTime;
        double finishTime;
        double cost;
        double distance;
        double passengerRideTime;
    };
    vector<Trip> schedule;

    Chromosome(int n) : giantTour(n), fitness(INF), numVehiclesUsed(0) { iota(giantTour.begin(), giantTour.end(), 0); }
};

int MAT_N = 0;
int MAT_V = 0;
vector<vector<double>> distMatrix;
map<string, int> matMap;
const string OFFICE_ID = "OFFICE";

void loadMatrix(const string &filename, int size) {
    distMatrix.assign(size, vector<double>(size, 0.0));
    ifstream fin(filename);
    if (!fin) {
        cerr << "Cannot open matrix file: " << filename << "\n";
        exit(1);
    }
    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++) fin >> distMatrix[i][j];

    cout << "[INFO] Loaded " << size << "x" << size << " distance matrix from " << filename << "\n";
}

void buildMatrixMap(const vector<Person> &persons, const vector<Driver> &drivers) {
    matMap.clear();
    for (int i = 0; i < (int)persons.size(); i++) matMap[persons[i].original_id] = i;

    for (int k = 0; k < (int)drivers.size(); k++) matMap[drivers[k].original_id] = MAT_N + k;

    matMap[OFFICE_ID] = MAT_N + MAT_V;
}

double getDistFromMatrix(const string &a, const string &b) { return distMatrix[matMap[a]][matMap[b]]; }

double getTravelTime(const string &a, const string &b, double speed_kmh) {
    double d = getDistFromMatrix(a, b);
    if (speed_kmh <= 0)
        if (d < 0.005)
            return 0;
        else
            return 1e7;
    return (d / speed_kmh) * 60.0;
}

double getDistPP(int i, int j) { return distMatrix[i][j]; }
double getDistPO(int i, int /*unused*/) { return distMatrix[i][MAT_N + MAT_V]; }
double getDistVP(int k, int i, int /*unused*/) { return distMatrix[MAT_N + k][i]; }
double getDistOP(int i, int /*unused*/) { return distMatrix[MAT_N + MAT_V][i]; }

void readMetadata(const string &filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "[WARN] Cannot open metadata file " << filename << ", using defaults.\n";
        return;
    }

    string line;
    getline(file, line);  // skip header

    map<string, string> meta;
    while (getline(file, line)) {
        stringstream ss(line);
        string key, value;
        getline(ss, key, ',');
        getline(ss, value, ',');
        meta[trim(key)] = trim(value);
    }
    file.close();

    if (meta.count("priority_1_max_delay_min")) PRIORITY_DELAY[1] = stoi(meta["priority_1_max_delay_min"]);
    if (meta.count("priority_2_max_delay_min")) PRIORITY_DELAY[2] = stoi(meta["priority_2_max_delay_min"]);
    if (meta.count("priority_3_max_delay_min")) PRIORITY_DELAY[3] = stoi(meta["priority_3_max_delay_min"]);
    if (meta.count("priority_4_max_delay_min")) PRIORITY_DELAY[4] = stoi(meta["priority_4_max_delay_min"]);
    if (meta.count("priority_5_max_delay_min")) PRIORITY_DELAY[5] = stoi(meta["priority_5_max_delay_min"]);
    PRIORITY_DELAY[0] = PRIORITY_DELAY[5];

    if (meta.count("objective_cost_weight")) OBJ_COST_WEIGHT = stod(meta["objective_cost_weight"]);
    if (meta.count("objective_time_weight")) OBJ_TIME_WEIGHT = stod(meta["objective_time_weight"]);

    cout << "[INFO] Metadata loaded from " << filename << "\n";
    cout << "  Priority delays: ";
    for (int i = 1; i <= 5; i++) cout << "P" << i << "=" << PRIORITY_DELAY[i] << " ";
    cout << "\n";
    cout << "  Weights: cost=" << OBJ_COST_WEIGHT << " time=" << OBJ_TIME_WEIGHT << "\n";
}

struct ImportedSolution {
    vector<int> giantTour;
    vector<pair<int, vector<int>>> trips;
    bool hasUnassigned = false;
};

vector<Driver> readVehicleCSV(const string &filename) {
    vector<Driver> drivers;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Cannot open " << filename << "\n";
        return drivers;
    }

    string line;
    getline(file, line);  // skip header

    int id = 0;
    while (getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        stringstream ss(line);
        string vehicle_id, fuel_type, vehicle_mode, seating_str, cost_str, speed_str, loc_x_str, loc_y_str,
            avail_time_str, category_str;

        getline(ss, vehicle_id, ',');
        getline(ss, fuel_type, ',');
        getline(ss, vehicle_mode, ',');
        getline(ss, seating_str, ',');
        getline(ss, cost_str, ',');
        getline(ss, speed_str, ',');
        getline(ss, loc_x_str, ',');
        getline(ss, loc_y_str, ',');
        getline(ss, avail_time_str, ',');
        getline(ss, category_str, ',');

        Driver d;
        d.id = id++;
        d.original_id = trim(vehicle_id);
        d.capacity = stoi(trim(seating_str));
        d.cost_per_km = stod(trim(cost_str));
        d.speed_kmph = stod(trim(speed_str));
        d.start_lat = stod(trim(loc_x_str));
        d.start_lng = stod(trim(loc_y_str));
        d.start_time = timeToMinutes(trim(avail_time_str));
        string cat = trim(category_str);
        d.category = (cat == "premium") ? PREMIUM : NORMAL;
        d.type_str = trim(fuel_type) + "/" + trim(vehicle_mode);

        drivers.push_back(d);
    }
    file.close();
    cout << "[INFO] Loaded " << drivers.size() << " vehicles from " << filename << "\n";
    return drivers;
}

vector<Person> readEmployeeCSV(const string &filename) {
    vector<Person> persons;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Cannot open " << filename << "\n";
        return persons;
    }

    string line;
    getline(file, line);  // skip header

    int id = 0;
    while (getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        stringstream ss(line);
        string emp_id, priority_str, pickup_x_str, pickup_y_str, dest_x_str, dest_y_str, time_start_str, time_end_str,
            veh_pref_str, share_pref_str;

        getline(ss, emp_id, ',');
        getline(ss, priority_str, ',');
        getline(ss, pickup_x_str, ',');
        getline(ss, pickup_y_str, ',');
        getline(ss, dest_x_str, ',');
        getline(ss, dest_y_str, ',');
        getline(ss, time_start_str, ',');
        getline(ss, time_end_str, ',');
        getline(ss, veh_pref_str, ',');
        getline(ss, share_pref_str);

        Person p;
        p.id = id++;
        p.original_id = trim(emp_id);
        p.priority = stoi(trim(priority_str));
        p.p_lat = stod(trim(pickup_x_str));
        p.p_lng = stod(trim(pickup_y_str));
        p.d_lat = stod(trim(dest_x_str));
        p.d_lng = stod(trim(dest_y_str));
        p.early_pickup = timeToMinutes(trim(time_start_str));
        p.late_drop = timeToMinutes(trim(time_end_str));
        if (p.late_drop - p.early_pickup < 0) p.late_drop += 1440;

        string veh_pref = trim(veh_pref_str);
        p.pref_vehicle = (veh_pref == "premium") ? PREMIUM : NORMAL;

        string share_pref = trim(share_pref_str);
        if (share_pref == "single")
            p.max_sharing = SINGLE;
        else if (share_pref == "double")
            p.max_sharing = DOUBLE;
        else if (share_pref == "triple")
            p.max_sharing = TRIPLE;
        else
            p.max_sharing = ANY;

        p.load = 1;
        persons.push_back(p);
    }
    file.close();
    cout << "[INFO] Loaded " << persons.size() << " employees from " << filename << "\n";
    return persons;
}

map<string, int> buildEmployeeIdMap(const vector<Person> &persons) {
    map<string, int> m;
    for (const auto &p : persons) m[p.original_id] = p.id;
    return m;
}

map<string, int> buildVehicleIdMap(const vector<Driver> &drivers) {
    map<string, int> m;
    for (const auto &d : drivers) m[d.original_id] = d.id;
    return m;
}

struct ParsedRow {
    int vIdx;
    int drop_mins;
    int pickup_mins;
    int empIdx;
};

ImportedSolution readCSVSolution(const string &filename, const vector<Person> &persons, const vector<Driver> &drivers) {
    ImportedSolution sol;
    map<string, int> empMap = buildEmployeeIdMap(persons);
    map<string, int> vehMap = buildVehicleIdMap(drivers);

    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Cannot open " << filename << "\n";
        return sol;
    }

    string line;
    getline(file, line);  // skip header
    getline(file, line);

    vector<ParsedRow> rows;
    set<int> seenEmp;

    while (getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        stringstream ss(line);
        string vehicle_id, category, employee_id, pickup_time, drop_time;
        getline(ss, vehicle_id, ',');
        getline(ss, category, ',');
        getline(ss, employee_id, ',');
        getline(ss, pickup_time, ',');
        getline(ss, drop_time, ',');

        vehicle_id = trim(vehicle_id);
        employee_id = trim(employee_id);

        if (!empMap.count(employee_id) || !vehMap.count(vehicle_id)) continue;

        int empIdx = empMap[employee_id];
        if (seenEmp.count(empIdx)) continue;

        seenEmp.insert(empIdx);

        rows.push_back({vehMap[vehicle_id], timeToMinutes(trim(drop_time)), timeToMinutes(trim(pickup_time)), empIdx});
    }
    file.close();

    sort(rows.begin(), rows.end(), [](const ParsedRow &a, const ParsedRow &b) {
        if (a.vIdx != b.vIdx) return a.vIdx < b.vIdx;
        if (a.drop_mins != b.drop_mins) return a.drop_mins < b.drop_mins;
        return a.pickup_mins < b.pickup_mins;
    });

    int current_vIdx = -1;
    int current_drop = -1;
    vector<int> current_emps;

    for (const auto &row : rows) {
        if (current_vIdx != row.vIdx || current_drop != row.drop_mins) {
            if (current_vIdx != -1) {
                sol.trips.push_back({current_vIdx, current_emps});
                for (int e : current_emps) sol.giantTour.push_back(e);
            }
            current_vIdx = row.vIdx;
            current_drop = row.drop_mins;
            current_emps.clear();
        }
        current_emps.push_back(row.empIdx);
    }

    if (current_vIdx != -1) {
        sol.trips.push_back({current_vIdx, current_emps});
        for (int e : current_emps) sol.giantTour.push_back(e);
    }

    for (const auto &p : persons) {
        if (!seenEmp.count(p.id)) {
            sol.hasUnassigned = true;
            sol.giantTour.push_back(p.id);
        }
    }

    return sol;
}

void evaluateImported(Chromosome &chromo, const ImportedSolution &imported, const vector<Person> &persons,
                      const vector<Driver> &drivers) {
    int N = persons.size();
    int M = drivers.size();
    // INFEASIBILITY_PENALTY = 0.0;
    chromo.fitness = 0.0;
    chromo.schedule.clear();

    map<int, bool> usedVehicles;
    map<int, double> vehicleTime;
    map<int, bool> firstTripForVehicle;

    for (int k = 0; k < M; k++) {
        vehicleTime[k] = (double)drivers[k].start_time;
        firstTripForVehicle[k] = true;
    }

    for (const auto &[vIdx, customers] : imported.trips) {
        if (customers.empty() || vIdx < 0 || vIdx >= M) continue;

        const Driver &d = drivers[vIdx];
        double availTime = vehicleTime[vIdx];
        bool isFirst = firstTripForVehicle[vIdx];

        int firstCust = customers[0];
        string fromId = isFirst ? drivers[vIdx].original_id : OFFICE_ID;
        double legDist = isFirst ? getDistVP(vIdx, firstCust, N) : getDistOP(firstCust, N);
        double travel = getTravelTime(fromId, persons[firstCust].original_id, d.speed_kmph);

        double currentVisTime = max(availTime + travel, (double)persons[firstCust].early_pickup);

        vector<double> pickupTimes;
        pickupTimes.push_back(currentVisTime);

        double routeDist = legDist;
        int tempPrev = firstCust;

        for (size_t k = 1; k < customers.size(); k++) {
            int pIdx = customers[k];
            double dist = getDistPP(tempPrev, pIdx);
            double trav = getTravelTime(persons[tempPrev].original_id, persons[pIdx].original_id, d.speed_kmph);
            double arrival = currentVisTime + trav;
            currentVisTime = max(arrival, (double)persons[pIdx].early_pickup);

            pickupTimes.push_back(currentVisTime);
            routeDist += dist;
            tempPrev = pIdx;
        }

        double toOffice = getDistPO(tempPrev, N);
        double travOffice = getTravelTime(persons[tempPrev].original_id, OFFICE_ID, d.speed_kmph);
        double arrivalOffice = currentVisTime + travOffice;
        routeDist += toOffice;

        // Penalty
        double penalty = 0.0;
        const double EPSILON = 5;
        int groupSize = customers.size();
        int runningLoad = 0;

        for (size_t k = 0; k < customers.size(); k++) {
            int pIdx = customers[k];
            runningLoad += persons[pIdx].load;

            double allowedLateDrop = (double)persons[pIdx].late_drop + (double)getPriorityDelay(persons[pIdx].priority);
            if (arrivalOffice > allowedLateDrop + EPSILON) {
                penalty += INFEASIBILITY_PENALTY * (arrivalOffice - allowedLateDrop);
                cout << persons[pIdx].original_id << " " << penalty << "late time penalty"
                     << "\n";
            }

            if (groupSize > persons[pIdx].max_sharing) {
                penalty += PENALTY_SHARING_PREFERENCE;
                cout << persons[pIdx].original_id << " " << penalty << "sharing penalty"
                     << "\n";
            }

            if (persons[pIdx].pref_vehicle == PREMIUM && d.category != PREMIUM) {
                penalty += PENALTY_PREMIUM_VEHICLE;
                cout << persons[pIdx].original_id << " " << penalty << "vehicle pref penalty"
                     << "\n";
            }

            if (runningLoad > d.capacity) {
                double segment_end_time = (k + 1 < customers.size()) ? pickupTimes[k + 1] : arrivalOffice;
                double segment_duration = segment_end_time - pickupTimes[k];

                double capPenalty = PENALTY_CAPACITY_EXCEEDED * (runningLoad - d.capacity) * segment_duration;
                penalty += capPenalty;

                cout << d.original_id << " overloaded by " << (runningLoad - d.capacity) << " for " << segment_duration
                     << " mins. Penalty: " << capPenalty << "\n";
            }
        }

        double totalRideTime = 0.0;
        for (size_t k = 0; k < customers.size(); k++) totalRideTime += arrivalOffice - pickupTimes[k];

        double monetaryCost = routeDist * d.cost_per_km;
        double tripCost = (monetaryCost * OBJ_COST_WEIGHT) + (totalRideTime * OBJ_TIME_WEIGHT) + penalty;

        chromo.fitness += tripCost;

        Chromosome::Trip trip;
        trip.vehicleIdx = vIdx;
        trip.customers = customers;
        trip.finishTime = arrivalOffice;
        trip.cost = tripCost;
        trip.distance = routeDist;
        trip.passengerRideTime = totalRideTime;
        chromo.schedule.push_back(trip);

        vehicleTime[vIdx] = arrivalOffice;
        firstTripForVehicle[vIdx] = false;
        usedVehicles[vIdx] = true;
    }

    chromo.numVehiclesUsed = (int)usedVehicles.size();
    // INFEASIBILITY_PENALTY = 10000.0;
}

struct Label {
    double cost;
    int pred;
    int vehicleIdx;
    double finishTime;
    vector<double> driverAvailTimes;
};

void splitProcedure(Chromosome &chromo, const vector<Person> &persons, const vector<Driver> &drivers) {
    int N = persons.size();
    int M = drivers.size();

    vector<Label> V(N + 1);
    V[0].cost = 0;
    V[0].pred = -1;
    V[0].driverAvailTimes.resize(M);
    for (int k = 0; k < M; k++) V[0].driverAvailTimes[k] = (double)drivers[k].start_time;

    for (int i = 1; i <= N; i++) {
        V[i].cost = INF;
        V[i].driverAvailTimes.assign(M, 0.0);
    }

    int maxCap = 0;
    for (const auto &d : drivers) maxCap = max(maxCap, d.capacity);

    for (int i = 0; i < N; i++) {
        if (V[i].cost >= INF) continue;

        double currentRouteDist = 0.0;
        int currentLoad = 0;

        for (int j = i + 1; j <= N && (j - i) <= maxCap; j++) {
            int custIdx = chromo.giantTour[j - 1];
            int prevCustIdx = (j - 1 == i) ? -1 : chromo.giantTour[j - 2];

            currentLoad += persons[custIdx].load;
            int currentGroupSize = (j - i);

            // bool sharingViolation = false;
            // for (int p = i; p < j; p++)
            // {
            //     if (currentGroupSize > persons[chromo.giantTour[p]].max_sharing)
            //     {
            //         sharingViolation = true;
            //         break;
            //     }
            // }
            // if (sharingViolation)
            //     break;
            double sharingPenalty = 0.0;
            for (int p = i; p < j; p++) {
                if (currentGroupSize > persons[chromo.giantTour[p]].max_sharing) {
                    sharingPenalty += PENALTY_SHARING_PREFERENCE;
                }
            }

            if (prevCustIdx == -1)
                currentRouteDist = 0.0;
            else
                currentRouteDist += getDistPP(prevCustIdx, custIdx);

            double distToOffice = getDistPO(custIdx, N);

            double bestTripCost = INF;
            int bestVehicle = -1;
            double bestFinishTime = -1.0;

            for (int k = 0; k < M; k++) {
                const Driver &d = drivers[k];
                if (d.capacity < currentLoad) continue;

                // bool prefFail = false;
                // for (int p = i; p < j; p++)
                // {
                //     if (persons[chromo.giantTour[p]].pref_vehicle == PREMIUM &&
                //         d.category != PREMIUM)
                //     {
                //         prefFail = true;
                //         break;
                //     }
                // }
                // if (prefFail)
                //     continue;
                double vehiclePenalty = 0.0;
                for (int p = i; p < j; p++) {
                    if (persons[chromo.giantTour[p]].pref_vehicle == PREMIUM && d.category != PREMIUM) {
                        vehiclePenalty += PENALTY_PREMIUM_VEHICLE;
                    }
                }

                int firstCust = chromo.giantTour[i];
                double availTime = V[i].driverAvailTimes[k];
                bool isFirstTrip = (availTime == (double)drivers[k].start_time);

                string fromId = isFirstTrip ? drivers[k].original_id : OFFICE_ID;
                double distStartToFirst = isFirstTrip ? getDistVP(k, firstCust, N) : getDistOP(firstCust, N);
                double totalDist = distStartToFirst + currentRouteDist + distToOffice;

                double timeToFirst = getTravelTime(fromId, persons[firstCust].original_id, d.speed_kmph);
                double currentVisTime = max(availTime + timeToFirst, (double)persons[firstCust].early_pickup);

                vector<double> pickupTimes;
                pickupTimes.push_back(currentVisTime);

                int tempPrev = firstCust;
                for (int p = i + 1; p < j; p++) {
                    int pIdx = chromo.giantTour[p];
                    double legTime =
                        getTravelTime(persons[tempPrev].original_id, persons[pIdx].original_id, d.speed_kmph);
                    double arrival = currentVisTime + legTime;
                    double startSrv = max(arrival, (double)persons[pIdx].early_pickup);

                    pickupTimes.push_back(startSrv);
                    currentVisTime = startSrv;
                    tempPrev = pIdx;
                }

                double travelToOffice = getTravelTime(persons[tempPrev].original_id, OFFICE_ID, d.speed_kmph);
                double arrivalAtOffice = currentVisTime + travelToOffice;

                double penalty = sharingPenalty + vehiclePenalty;
                const double EPSILON = 1e-5;
                int runningLoad = 0;

                for (int p = i; p < j; p++) {
                    int pIdx = chromo.giantTour[p];
                    double actualPickup = pickupTimes[p - i];
                    runningLoad += persons[pIdx].load;

                    // 1. Check Late Drop Penalty
                    double allowedLateDrop =
                        (double)persons[pIdx].late_drop + (double)getPriorityDelay(persons[pIdx].priority);
                    if (arrivalAtOffice > allowedLateDrop + EPSILON)
                        penalty += INFEASIBILITY_PENALTY * (arrivalAtOffice - allowedLateDrop);

                    if (runningLoad > d.capacity) {
                        double segment_start = pickupTimes[p - i];
                        double segment_end = (p + 1 < j) ? pickupTimes[p - i + 1] : arrivalAtOffice;
                        double segment_duration = segment_end - segment_start;

                        penalty += PENALTY_CAPACITY_EXCEEDED * (runningLoad - d.capacity) * segment_duration;
                    }
                }

                double totalPassengerRideTime = 0.0;
                for (int p = i; p < j; p++) totalPassengerRideTime += arrivalAtOffice - pickupTimes[p - i];

                double monetaryCost = totalDist * d.cost_per_km;
                double cost = (monetaryCost * OBJ_COST_WEIGHT) + (totalPassengerRideTime * OBJ_TIME_WEIGHT) + penalty;

                if (cost < bestTripCost) {
                    bestTripCost = cost;
                    bestVehicle = k;
                    bestFinishTime = arrivalAtOffice;
                }
            }

            if (bestVehicle != -1) {
                double newCost = V[i].cost + bestTripCost;
                if (newCost < V[j].cost) {
                    V[j].cost = newCost;
                    V[j].pred = i;
                    V[j].vehicleIdx = bestVehicle;
                    V[j].finishTime = bestFinishTime;
                    V[j].driverAvailTimes = V[i].driverAvailTimes;
                    V[j].driverAvailTimes[bestVehicle] = bestFinishTime;
                }
            }
        }
    }

    chromo.fitness = V[N].cost;
    if (chromo.fitness >= INF) return;

    int curr = N;
    map<int, bool> usedVehicles;

    while (curr > 0) {
        int prev = V[curr].pred;
        int vIdx = V[curr].vehicleIdx;

        Chromosome::Trip trip;
        trip.vehicleIdx = vIdx;
        trip.finishTime = V[curr].finishTime;
        for (int k = prev; k < curr; k++) trip.customers.push_back(chromo.giantTour[k]);

        chromo.schedule.push_back(trip);
        usedVehicles[vIdx] = true;
        curr = prev;
    }

    chromo.numVehiclesUsed = (int)usedVehicles.size();
    reverse(chromo.schedule.begin(), chromo.schedule.end());
}

Chromosome crossover(const Chromosome &p1, const Chromosome &p2, mt19937 &rng) {
    int n = p1.giantTour.size();
    Chromosome child(n);
    vector<bool> present(n, false);

    int start = rng() % n;
    int end = rng() % n;
    if (start > end) swap(start, end);

    for (int i = start; i <= end; i++) {
        child.giantTour[i] = p1.giantTour[i];
        present[p1.giantTour[i]] = true;
    }

    int curr = (end + 1) % n;
    int p2_idx = (end + 1) % n;
    while (curr != start) {
        int gene = p2.giantTour[p2_idx];
        if (!present[gene]) {
            child.giantTour[curr] = gene;
            curr = (curr + 1) % n;
        }
        p2_idx = (p2_idx + 1) % n;
    }
    return child;
}

void mutate(Chromosome &c, mt19937 &rng) {
    int n = c.giantTour.size();
    int i = rng() % n;
    int j = rng() % n;
    swap(c.giantTour[i], c.giantTour[j]);
}

struct SolutionMetrics {
    double totalDistance;
    double totalDuration;
    double totalPassengerRideTime;
    double monetaryCost;
    double weightedObjective;
};

SolutionMetrics calculateDetailedMetrics(const Chromosome &best, const vector<Person> &persons,
                                         const vector<Driver> &drivers) {
    SolutionMetrics metrics = {0, 0, 0, 0, 0};
    int N = persons.size();

    map<int, vector<Chromosome::Trip>> driverTrips;
    for (const auto &t : best.schedule) driverTrips[t.vehicleIdx].push_back(t);

    for (auto &dt : driverTrips) {
        int vIdx = dt.first;
        const Driver &d = drivers[vIdx];
        double vehicleStart = (double)d.start_time;
        double currentTime = vehicleStart;

        for (int i = 0; i < (int)dt.second.size(); i++) {
            auto &trip = dt.second[i];
            int firstCust = trip.customers[0];

            string fromId = (i == 0) ? drivers[vIdx].original_id : OFFICE_ID;
            double legDist = (i == 0) ? getDistVP(vIdx, firstCust, N) : getDistOP(firstCust, N);
            double travel = getTravelTime(fromId, persons[firstCust].original_id, d.speed_kmph);

            currentTime = max(currentTime + travel, (double)persons[firstCust].early_pickup);
            metrics.totalDistance += legDist;

            int prevCust = firstCust;
            vector<double> pickupTimes;
            pickupTimes.push_back(currentTime);

            for (size_t k = 1; k < trip.customers.size(); k++) {
                int cIdx = trip.customers[k];
                double dist = getDistPP(prevCust, cIdx);
                double trav = getTravelTime(persons[prevCust].original_id, persons[cIdx].original_id, d.speed_kmph);
                currentTime = max(currentTime + trav, (double)persons[cIdx].early_pickup);

                pickupTimes.push_back(currentTime);
                metrics.totalDistance += dist;
                prevCust = cIdx;
            }

            double toOffice = getDistPO(prevCust, N);
            double travOffice = getTravelTime(persons[prevCust].original_id, OFFICE_ID, d.speed_kmph);
            double arrivalOffice = currentTime + travOffice;
            metrics.totalDistance += toOffice;

            for (size_t k = 0; k < trip.customers.size(); k++)
                metrics.totalPassengerRideTime += arrivalOffice - pickupTimes[k];

            currentTime = arrivalOffice;
        }

        metrics.totalDuration += (currentTime - vehicleStart);
    }

    metrics.monetaryCost = 0.0;
    for (auto &dt : driverTrips) {
        int vIdx = dt.first;
        const Driver &d = drivers[vIdx];
        double vehDist = 0.0;

        for (int i = 0; i < (int)dt.second.size(); i++) {
            auto &trip = dt.second[i];
            int firstCust = trip.customers[0];
            vehDist += (i == 0) ? getDistVP(vIdx, firstCust, N) : getDistOP(firstCust, N);

            int prevCust = firstCust;
            for (size_t k = 1; k < trip.customers.size(); k++) {
                int cIdx = trip.customers[k];
                vehDist += getDistPP(prevCust, cIdx);
                prevCust = cIdx;
            }
            vehDist += getDistPO(prevCust, N);
        }
        metrics.monetaryCost += vehDist * d.cost_per_km;
    }

    metrics.weightedObjective =
        (metrics.monetaryCost * OBJ_COST_WEIGHT) + (metrics.totalPassengerRideTime * OBJ_TIME_WEIGHT);
    return metrics;
}

void writeCSVOutput(const string &filename, const Chromosome &solution, const vector<Person> &persons,
                    const vector<Driver> &drivers, double cost_no_penalty, double penalty) {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Cannot create " << filename << "\n";
        return;
    }
    file << cost_no_penalty << "," << penalty << "\n";
    file << "vehicle_id,category,employee_id,pickup_time,drop_time\n";

    int N = persons.size();
    map<int, vector<Chromosome::Trip>> driverTrips;
    for (const auto &t : solution.schedule) driverTrips[t.vehicleIdx].push_back(t);

    for (auto &dt : driverTrips) {
        int vIdx = dt.first;
        const Driver &d = drivers[vIdx];
        double currentTime = (double)d.start_time;

        for (int i = 0; i < (int)dt.second.size(); i++) {
            auto &trip = dt.second[i];
            int firstCust = trip.customers[0];

            string fromId = (i == 0) ? drivers[vIdx].original_id : OFFICE_ID;
            double travel = getTravelTime(fromId, persons[firstCust].original_id, d.speed_kmph);
            currentTime = max(currentTime + travel, (double)persons[firstCust].early_pickup);

            vector<double> pickupTimes;
            pickupTimes.push_back(currentTime);
            int prevCust = firstCust;

            for (size_t k = 1; k < trip.customers.size(); k++) {
                int cIdx = trip.customers[k];
                double trav = getTravelTime(persons[prevCust].original_id, persons[cIdx].original_id, d.speed_kmph);
                currentTime = max(currentTime + trav, (double)persons[cIdx].early_pickup);
                pickupTimes.push_back(currentTime);
                prevCust = cIdx;
            }

            double travOffice = getTravelTime(persons[prevCust].original_id, OFFICE_ID, d.speed_kmph);
            double arrivalOffice = currentTime + travOffice;

            for (size_t k = 0; k < trip.customers.size(); k++) {
                int empIdx = trip.customers[k];
                file << d.original_id << "," << (d.category == PREMIUM ? "Premium" : "Normal") << ","
                     << persons[empIdx].original_id << "," << minToTime((int)round(pickupTimes[k])) << ","
                     << minToTime((int)round(arrivalOffice)) << "\n";
            }

            currentTime = arrivalOffice;
        }
    }
    file.close();
}

void writeBaseline(const string &filename, const string &baseline, const Chromosome &solution,
                   const vector<Person> &persons, const vector<Driver> &drivers) {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Cannot create " << filename << "\n";
        return;
    }

    // Structure to store results for sorting
    struct BaselineEntry {
        string id;
        double cost;
        int time;
    };
    vector<BaselineEntry> entries;

    int N = persons.size();
    map<int, vector<Chromosome::Trip>> driverTrips;
    for (const auto &t : solution.schedule) driverTrips[t.vehicleIdx].push_back(t);

    for (auto &dt : driverTrips) {
        int vIdx = dt.first;
        const Driver &d = drivers[vIdx];
        double currentTime = (double)d.start_time;

        for (int i = 0; i < (int)dt.second.size(); i++) {
            auto &trip = dt.second[i];
            int numPax = trip.customers.size();

            // Calculate actual trip distance
            int firstC = trip.customers[0];
            double tripDist = (i == 0) ? getDistVP(vIdx, firstC, N) : getDistOP(firstC, N);

            string fromId = (i == 0) ? d.original_id : OFFICE_ID;
            double travel = getTravelTime(fromId, persons[firstC].original_id, d.speed_kmph);
            currentTime = max(currentTime + travel, (double)persons[firstC].early_pickup);

            vector<double> pickupTimes;
            pickupTimes.push_back(currentTime);
            int prevCust = firstC;

            for (size_t k = 1; k < trip.customers.size(); k++) {
                int cIdx = trip.customers[k];
                tripDist += getDistPP(prevCust, cIdx);
                double t_leg = getTravelTime(persons[prevCust].original_id, persons[cIdx].original_id, d.speed_kmph);
                currentTime = max(currentTime + t_leg, (double)persons[cIdx].early_pickup);
                pickupTimes.push_back(currentTime);
                prevCust = cIdx;
            }

            tripDist += getDistPO(prevCust, N);
            double arrivalOffice = currentTime + getTravelTime(persons[prevCust].original_id, OFFICE_ID, d.speed_kmph);

            // Real monetary cost = distance * cost_per_km
            double realTripMonetaryCost = tripDist * d.cost_per_km;
            double sharePerPerson = realTripMonetaryCost / (double)numPax;

            for (size_t k = 0; k < trip.customers.size(); k++) {
                int empIdx = trip.customers[k];
                int rideTime = (int)round(arrivalOffice - pickupTimes[k]);

                entries.push_back({persons[empIdx].original_id, sharePerPerson, rideTime});
            }
            currentTime = arrivalOffice;
        }
    }

    // Sort entries by employee ID (e.g., E01, E02...)
    sort(entries.begin(), entries.end(), [](const BaselineEntry &a, const BaselineEntry &b) { return a.id < b.id; });

    // --- NEW: Read original baseline file and calculate savings safely ---
    map<string, pair<double, int>> baselineData;
    ifstream baseFile(baseline);
    if (baseFile.is_open()) {
        string line;
        getline(baseFile, line);  // Skip the header row
        int rowCount = 2;         // Keep track for error logging

        while (getline(baseFile, line)) {
            // Strip trailing carriage return if it exists
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) {
                rowCount++;
                continue;
            }

            stringstream ss(line);
            string id, costStr, timeStr;

            // Check if all 3 columns exist
            if (getline(ss, id, ',') && getline(ss, costStr, ',') && getline(ss, timeStr, ',')) {
                id = trim(id);
                try {
                    // Try to convert, catch if it fails (e.g. empty, "N/A", text)
                    double cost = stod(trim(costStr));
                    int time = stoi(trim(timeStr));
                    baselineData[id] = {cost, time};
                } catch (const std::exception &e) {
                    cerr << "[WARN] Invalid data in baseline.csv at row " << rowCount << " for ID " << id
                         << ". Skipping. Error: " << e.what() << "\n";
                }
            } else {
                cerr << "[WARN] Missing columns in baseline.csv at row " << rowCount << ". Skipping.\n";
            }
            rowCount++;
        }
        baseFile.close();
    } else {
        cerr << "[WARN] Could not open baseline.csv. Savings will be calculated as 0.\n";
    }

    double totalCostSaved = 0.0;
    int totalTimeSaved = 0;
    int matchedRecords = 0;  // Track how many we actually compared

    for (const auto &entry : entries) {
        if (baselineData.find(entry.id) != baselineData.end()) {
            // Savings = Original Baseline - Current Solution
            totalCostSaved += (baselineData[entry.id].first - entry.cost);
            totalTimeSaved += (baselineData[entry.id].second - entry.time);
            matchedRecords++;
        }
    }

    // Warn the user if the data was partial
    if (matchedRecords != entries.size()) {
        cout << "[WARN] Baseline comparison is partial. Matched " << matchedRecords << " out of " << entries.size()
             << " employees.\n";
    }
    // --------------------------------------------------------------

    // Write to CSV
    // 1. Output the savings on the first line (Added a comma so it remains valid CSV)
    file << fixed << setprecision(3) << totalCostSaved << "," << totalTimeSaved << "\n";

    // 2. Output standard headers
    file << "employee_id,cost,time_min\n";

    // 3. Output the individual data
    for (const auto &entry : entries) {
        file << entry.id << "," << fixed << setprecision(5) << entry.cost << "," << entry.time << "\n";
    }

    file.close();
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <test_case_folder_path>\n";
        return 1;
    }

    string basePath = argv[1];
    if (basePath.back() != '/') basePath += '/';

    cout << "[INFO] Reading metadata...\n";
    readMetadata(basePath + "metadata.csv");

    cout << "[INFO] Reading input data...\n";
    vector<Driver> drivers = readVehicleCSV(basePath + "vehicles.csv");
    vector<Person> persons = readEmployeeCSV(basePath + "employees.csv");

    if (drivers.empty() || persons.empty()) {
        cerr << "Error: Failed to load vehicle or employee data!\n";
        return 1;
    }
    double max_cost = 0.0;
    for (const auto &d : drivers) {
        max_cost = max(max_cost, d.cost_per_km);
    }
    INFEASIBILITY_PENALTY = max_cost * 1000.0;

    MAT_N = (int)persons.size();
    MAT_V = (int)drivers.size();
    int matSize = MAT_N + MAT_V + 1;

    cout << "[INFO] Loading distance matrix...\n";
    loadMatrix(basePath + "matrix.txt", matSize);
    buildMatrixMap(persons, drivers);

    vector<string> subfolders = {
        "ALNS", "Branch-And-Cut", "god", "Heterogeneous_DARP"
        //"Variable_Neighbourhood_Search",
        // "Clustering-Routing-DP-Solver"
    };

    cout << "\n[INFO] Loading initial population (exact vehicle assignments preserved)...\n";
    vector<Chromosome> population;

    for (const auto &folder : subfolders) {
        string filepath = basePath + folder + "/output_vehicle.csv";
        ifstream testFile(filepath);
        if (!testFile.is_open()) {
            cerr << "[WARN] Skipping " << filepath << " (not found)\n";
            continue;
        }
        testFile.close();

        ImportedSolution imported = readCSVSolution(filepath, persons, drivers);
        if (imported.hasUnassigned) {
            cout << "  [" << folder << "] Skipped: Solution has unassigned people.\n";
            continue;
        }

        Chromosome c(persons.size());
        c.giantTour = imported.giantTour;
        evaluateImported(c, imported, persons, drivers);

        population.push_back(c);
        cout << "  [" << folder << "] Base fitness: " << fixed << setprecision(4) << c.fitness
             << " | Vehicles: " << c.numVehiclesUsed << "\n";
    }
    mt19937 init_rng(chrono::steady_clock::now().time_since_epoch().count());
    for (int i = 0; i < 15; i++) {
        Chromosome c_rand(persons.size());
        shuffle(c_rand.giantTour.begin(), c_rand.giantTour.end(), init_rng);
        splitProcedure(c_rand, persons, drivers);
        population.push_back(c_rand);
    }

    if (population.empty()) {
        cerr << "Error: No initial solutions loaded!\n";
        return 1;
    }

    sort(population.begin(), population.end(),
         [](const Chromosome &a, const Chromosome &b) { return a.fitness < b.fitness; });

    double bestFitnessEver = population[0].fitness;
    Chromosome bestEver = population[0];

    cout << "\n[INFO] Best initial fitness: " << fixed << setprecision(4) << bestFitnessEver << "\n";
    cout << "[INFO] Starting GA — will only improve from here.\n\n";

    cout << "[INFO] Writing best initial solution to CSV for verification...\n";
    SolutionMetrics init_metrics = calculateDetailedMetrics(bestEver, persons, drivers);
    double init_penalty = bestEver.fitness - init_metrics.weightedObjective;
    writeCSVOutput(basePath + "memetic_algorithm/initial_best_output_vehicle.csv", bestEver, persons, drivers,
                   init_metrics.weightedObjective, init_penalty);
    cout << "[INFO] Initial best written to: memetic_algorithm/initial_best_output_vehicle.csv\n\n";

    int POP_SIZE = population.size();
    int GENERATIONS = 20000;
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

    auto start_time = chrono::high_resolution_clock::now();

    for (int gen = 0; gen < GENERATIONS; gen++) {
        auto current_time = chrono::high_resolution_clock::now();
        if (chrono::duration_cast<chrono::seconds>(current_time - start_time).count() >= 2) {
            break;
        }
        sort(population.begin(), population.end(),
             [](const Chromosome &a, const Chromosome &b) { return a.fitness < b.fitness; });

        if (population[0].fitness < bestFitnessEver) {
            bestFitnessEver = population[0].fitness;
            bestEver = population[0];
            cout << "  [Gen " << gen << "] New best: " << fixed << setprecision(4) << bestFitnessEver << "\n";
        }

        if (gen % 50 == 0)
            cout << "Gen " << gen << " | Current best: " << fixed << setprecision(4) << population[0].fitness
                 << " | All-time best: " << bestFitnessEver << "\n";

        vector<Chromosome> nextPop;
        nextPop.push_back(population[0]);

        while ((int)nextPop.size() < POP_SIZE) {
            int t1 = rng() % POP_SIZE;
            int t2 = rng() % POP_SIZE;

            Chromosome child = crossover(population[t1], population[t2], rng);
            if (rng() % 100 < 15) mutate(child, rng);

            splitProcedure(child, persons, drivers);
            nextPop.push_back(child);
        }
        population = nextPop;
    }

    sort(population.begin(), population.end(),
         [](const Chromosome &a, const Chromosome &b) { return a.fitness < b.fitness; });
    if (population[0].fitness < bestFitnessEver) {
        bestFitnessEver = population[0].fitness;
        bestEver = population[0];
    }

    auto end_time = chrono::high_resolution_clock::now();
    double duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count() / 1000.0;

    SolutionMetrics metrics = calculateDetailedMetrics(bestEver, persons, drivers);
    double penaltyComponent = bestEver.fitness - metrics.weightedObjective;

    cout << "\n[INFO] Writing final solution...\n";
    writeCSVOutput(basePath + "memetic_algorithm/output_vehicle.csv", bestEver, persons, drivers,
                   metrics.weightedObjective, penaltyComponent);

    writeBaseline(basePath + "memetic_algorithm/output_employees.csv", basePath + "baseline.csv", bestEver, persons,
                  drivers);
    cout << "\n" << string(80, '=') << "\n";
    cout << "                    FINAL METRICS BREAKDOWN\n";
    cout << string(80, '=') << "\n\n";

    cout << left << setw(35) << "Total Distance:" << fixed << setprecision(4) << metrics.totalDistance << " km\n";
    cout << left << setw(35) << "Total Duration:" << fixed << setprecision(4) << metrics.totalDuration << " min\n";
    cout << left << setw(35) << "Total Passenger Ride Time:" << fixed << setprecision(4)
         << metrics.totalPassengerRideTime << " min\n";
    cout << left << setw(35) << "Total Monetary Cost:" << fixed << setprecision(4) << metrics.monetaryCost << "\n\n";

    cout << string(80, '=') << "\n";
    cout << "WEIGHTED OBJECTIVE:\n";
    cout << "  (" << fixed << setprecision(4) << metrics.monetaryCost << " * " << OBJ_COST_WEIGHT << ") + ("
         << metrics.totalPassengerRideTime << " * " << OBJ_TIME_WEIGHT << ") = " << metrics.weightedObjective << "\n";

    penaltyComponent = bestEver.fitness - metrics.weightedObjective;
    if (penaltyComponent > 1.0)
        cout << "[WARN] Penalty present: " << penaltyComponent << "\n";
    else
        cout << "[OK] Solution is feasible.\n";

    cout << string(80, '=') << "\n\n";
    cout << left << setw(35) << "Execution Time:" << fixed << setprecision(3) << duration << " seconds\n";
    cout << left << setw(35) << "Vehicles Used:" << bestEver.numVehiclesUsed << "\n\n";

    // Detailed route
    map<int, vector<Chromosome::Trip>> driverTrips;
    for (const auto &t : bestEver.schedule) driverTrips[t.vehicleIdx].push_back(t);

    cout << string(80, '=') << "\n";
    cout << "                    DETAILED ROUTE SCHEDULE\n";
    cout << string(80, '=') << "\n";

    for (auto &dt : driverTrips) {
        int vIdx = dt.first;
        const Driver &d = drivers[vIdx];
        cout << "\nVehicle " << d.original_id << " (" << d.type_str << ")"
             << " | Capacity: " << d.capacity << " | Cost/km: " << d.cost_per_km << "\n";

        double currentTime = (double)d.start_time;
        double vehicleTotalDist = 0.0;

        for (int i = 0; i < (int)dt.second.size(); i++) {
            auto &trip = dt.second[i];
            int firstCust = trip.customers[0];

            string fromId = (i == 0) ? drivers[vIdx].original_id : OFFICE_ID;
            double legDist =
                (i == 0) ? getDistVP(vIdx, firstCust, persons.size()) : getDistOP(firstCust, persons.size());
            double travel = getTravelTime(fromId, persons[firstCust].original_id, d.speed_kmph);
            currentTime = max(currentTime + travel, (double)persons[firstCust].early_pickup);

            cout << "  Trip " << i + 1 << " (" << trip.customers.size() << " pax): ";
            cout << (i == 0 ? "[Base" : "[Office") << " -> " << persons[firstCust].original_id << " @"
                 << minToTime((int)round(currentTime)) << "]";

            vehicleTotalDist += legDist;
            int prevCust = firstCust;

            for (size_t k = 1; k < trip.customers.size(); k++) {
                int cIdx = trip.customers[k];
                double dist = getDistPP(prevCust, cIdx);
                double trav = getTravelTime(persons[prevCust].original_id, persons[cIdx].original_id, d.speed_kmph);
                currentTime = max(currentTime + trav, (double)persons[cIdx].early_pickup);

                cout << " -> " << persons[cIdx].original_id << " @" << minToTime((int)round(currentTime));
                vehicleTotalDist += dist;
                prevCust = cIdx;
            }

            double toOffice = getDistPO(prevCust, persons.size());
            double travOff = getTravelTime(persons[prevCust].original_id, OFFICE_ID, d.speed_kmph);
            currentTime += travOff;
            vehicleTotalDist += toOffice;

            cout << " -> OFFICE @" << minToTime((int)round(currentTime)) << "\n";
        }
        cout << "  Distance: " << fixed << setprecision(4) << vehicleTotalDist
             << " km | Cost: " << (vehicleTotalDist * d.cost_per_km) << "\n";
    }

    cout << "\n" << string(80, '=') << "\n";

    return 0;
}