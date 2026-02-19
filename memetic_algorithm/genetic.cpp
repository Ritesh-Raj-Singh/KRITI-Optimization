#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include <random>
#include <algorithm>
#include <chrono>
#include <string>
#include <iomanip>
#include <map>
#include <fstream>
#include <sstream>
#include <set>
#include <numeric>

using namespace std;

// =================== CONFIGURATION ===================

const double INF = 1e15;

// These will be loaded from metadata.csv
double OBJ_COST_WEIGHT = 0.7;
double OBJ_TIME_WEIGHT = 0.3;
int PRIORITY_DELAY[6] = {30, 5, 10, 15, 20, 30}; // index 0 = default, 1-5 = priorities

string trim(const string &str)
{
    string s = str;
    s.erase(remove(s.begin(), s.end(), '\r'), s.end());
    s.erase(remove(s.begin(), s.end(), '\n'), s.end());
    size_t first = s.find_first_not_of(" \t");
    if (first == string::npos)
        return "";
    size_t last = s.find_last_not_of(" \t");
    return s.substr(first, (last - first + 1));
}

int getPriorityDelay(int priority)
{
    if (priority >= 1 && priority <= 5)
        return PRIORITY_DELAY[priority];
    return PRIORITY_DELAY[0];
}

const double INFEASIBILITY_PENALTY = 10000.0;

// =================== ENUMS & HELPERS ===================

enum VehicleCat
{
    NORMAL = 0,
    PREMIUM = 1
};

int timeToMinutes(const string &t)
{
    int hh = stoi(t.substr(0, 2));
    int mm = stoi(t.substr(3, 2));
    return hh * 60 + mm;
}

string minToTime(double m_val)
{
    int m = (int)m_val;
    int hh = (m / 60) % 24;
    int mm = m % 60;
    string h_s = (hh < 10 ? "0" : "") + to_string(hh);
    string m_s = (mm < 10 ? "0" : "") + to_string(mm);
    return h_s + ":" + m_s;
}

// =================== O(1) FAST DISTANCE MATRIX ===================

int MAT_N = 0;
int MAT_V = 0;
int OFFICE_IDX = 0;
vector<vector<double>> distMatrix;

void loadMatrix(const string &filename, int size)
{
    distMatrix.assign(size, vector<double>(size, 0.0));
    ifstream fin(filename);
    if (!fin)
    {
        cerr << "Cannot open matrix file: " << filename << "\n";
        exit(1);
    }
    for (int i = 0; i < size; i++)
        for (int j = 0; j < size; j++)
            fin >> distMatrix[i][j];

    cout << "[INFO] Loaded " << size << "x" << size << " distance matrix from " << filename << "\n";
}

inline double fastDist(int idxA, int idxB)
{
    return distMatrix[idxA][idxB];
}

inline double fastTime(int idxA, int idxB, double speed_kmh)
{
    double d = distMatrix[idxA][idxB];
    return (d < 0.005) ? 0.0 : (d / speed_kmh) * 60.0;
}

// =================== METADATA READER ===================

void readMetadata(const string &filename)
{
    ifstream file(filename);
    if (!file.is_open())
    {
        cerr << "[WARN] Cannot open metadata file " << filename << ", using defaults." << endl;
        return;
    }

    string line;
    getline(file, line);

    map<string, string> meta;
    while (getline(file, line))
    {
        stringstream ss(line);
        string key, value;
        getline(ss, key, ',');
        getline(ss, value, ',');
        meta[trim(key)] = trim(value);
    }
    file.close();

    if (meta.count("priority_1_max_delay_min"))
        PRIORITY_DELAY[1] = stoi(meta["priority_1_max_delay_min"]);
    if (meta.count("priority_2_max_delay_min"))
        PRIORITY_DELAY[2] = stoi(meta["priority_2_max_delay_min"]);
    if (meta.count("priority_3_max_delay_min"))
        PRIORITY_DELAY[3] = stoi(meta["priority_3_max_delay_min"]);
    if (meta.count("priority_4_max_delay_min"))
        PRIORITY_DELAY[4] = stoi(meta["priority_4_max_delay_min"]);
    if (meta.count("priority_5_max_delay_min"))
        PRIORITY_DELAY[5] = stoi(meta["priority_5_max_delay_min"]);
    PRIORITY_DELAY[0] = PRIORITY_DELAY[5];

    if (meta.count("objective_cost_weight"))
        OBJ_COST_WEIGHT = stod(meta["objective_cost_weight"]);
    if (meta.count("objective_time_weight"))
        OBJ_TIME_WEIGHT = stod(meta["objective_time_weight"]);
}

// =================== DATA STRUCTURES ===================

enum ShareType
{
    SINGLE = 1,
    DOUBLE = 2,
    TRIPLE = 3,
    ANY = 99
};

struct Person
{
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

struct Driver
{
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

class Chromosome
{
public:
    vector<int> giantTour;
    double fitness;
    int numVehiclesUsed;

    struct Trip
    {
        int vehicleIdx;
        vector<int> customers;
        double startTime;
        double finishTime;
        double cost;
        double distance;
        double passengerRideTime;
    };
    vector<Trip> schedule;

    Chromosome(int n) : giantTour(n), fitness(INF), numVehiclesUsed(0)
    {
        iota(giantTour.begin(), giantTour.end(), 0);
    }
};

// =================== EXACT CSV EVALUATOR ===================
// NEW: Evaluates the literal routes provided in a CSV to get the exact cost.
double evaluateExactSchedule(Chromosome &chromo, const vector<Person> &persons, const vector<Driver> &drivers)
{
    double totalFitness = 0.0;

    for (auto &trip : chromo.schedule)
    {
        if (trip.customers.empty())
            continue;

        int vIdx = trip.vehicleIdx;
        const Driver &d = drivers[vIdx];

        double currentTime = d.start_time;
        double vehicleDist = 0.0;
        double passengerTime = 0.0;
        double penalty = 0.0;

        int firstCust = trip.customers[0];
        int vehIdxMatrix = MAT_N + vIdx;

        double timeToFirst = fastTime(vehIdxMatrix, firstCust, d.speed_kmph);
        double distToFirst = fastDist(vehIdxMatrix, firstCust);

        currentTime = max((double)d.start_time + timeToFirst, (double)persons[firstCust].early_pickup);
        vehicleDist += distToFirst;

        vector<double> pickupTimes;
        pickupTimes.push_back(currentTime);

        int prevCust = firstCust;
        int currentGroupSize = trip.customers.size();

        // Capacity & Sharing penalties
        if (currentGroupSize > d.capacity)
            penalty += INFEASIBILITY_PENALTY * (currentGroupSize - d.capacity);

        for (int cust : trip.customers)
        {
            if (persons[cust].pref_vehicle == PREMIUM && d.category != PREMIUM)
                penalty += INFEASIBILITY_PENALTY;
            if (currentGroupSize > persons[cust].max_sharing)
                penalty += INFEASIBILITY_PENALTY;
        }

        for (size_t k = 1; k < trip.customers.size(); k++)
        {
            int cIdx = trip.customers[k];
            double travel = fastTime(prevCust, cIdx, d.speed_kmph);
            double dist = fastDist(prevCust, cIdx);

            currentTime = max(currentTime + travel, (double)persons[cIdx].early_pickup);
            pickupTimes.push_back(currentTime);
            vehicleDist += dist;
            prevCust = cIdx;
        }

        double travelOffice = fastTime(prevCust, OFFICE_IDX, d.speed_kmph);
        double distOffice = fastDist(prevCust, OFFICE_IDX);
        double arrivalAtOffice = currentTime + travelOffice;
        vehicleDist += distOffice;

        for (size_t k = 0; k < trip.customers.size(); k++)
        {
            int cIdx = trip.customers[k];
            double actualPickup = pickupTimes[k];
            passengerTime += (arrivalAtOffice - actualPickup);

            if (arrivalAtOffice > persons[cIdx].late_drop)
            {
                penalty += INFEASIBILITY_PENALTY * (arrivalAtOffice - persons[cIdx].late_drop);
            }

            double directTime = fastTime(cIdx, OFFICE_IDX, d.speed_kmph);
            double delay = (arrivalAtOffice - actualPickup) - directTime;
            int allowedDelay = getPriorityDelay(persons[cIdx].priority);

            if (delay > allowedDelay)
            {
                penalty += INFEASIBILITY_PENALTY * (delay - allowedDelay);
            }
        }

        double tripCost = (vehicleDist * d.cost_per_km * OBJ_COST_WEIGHT) + (passengerTime * OBJ_TIME_WEIGHT) + penalty;
        totalFitness += tripCost;
        trip.finishTime = arrivalAtOffice;
    }
    return totalFitness;
}

// =================== SPLIT PROCEDURE ===================

struct Label
{
    double cost;
    int pred;
    int vehicleIdx;
    double finishTime;
    vector<double> driverAvailTimes;
};

void splitProcedure(Chromosome &chromo, const vector<Person> &persons, const vector<Driver> &drivers)
{
    int N = persons.size();
    int M = drivers.size();

    vector<Label> V(N + 1);

    V[0].cost = 0;
    V[0].pred = -1;
    V[0].driverAvailTimes.resize(M);
    for (int k = 0; k < M; k++)
        V[0].driverAvailTimes[k] = (double)drivers[k].start_time;

    for (int i = 1; i <= N; i++)
    {
        V[i].cost = INF;
        V[i].driverAvailTimes.assign(M, 0.0);
    }

    for (int i = 0; i < N; i++)
    {
        if (V[i].cost >= INF)
            continue;

        int maxCap = 0;
        for (const auto &d : drivers)
            maxCap = max(maxCap, d.capacity);

        double currentRouteDist = 0;
        int currentLoad = 0;

        for (int j = i + 1; j <= N && (j - i) <= maxCap; j++)
        {
            int custIdx = chromo.giantTour[j - 1];
            int prevCustIdx = (j - 1 == i) ? -1 : chromo.giantTour[j - 2];

            currentLoad += persons[custIdx].load;
            int currentGroupSize = (j - i);
            bool sharingViolation = false;

            for (int p = i; p < j; p++)
            {
                int memberIdx = chromo.giantTour[p];
                if (currentGroupSize > persons[memberIdx].max_sharing)
                {
                    sharingViolation = true;
                    break;
                }
            }
            if (sharingViolation)
                break;

            if (prevCustIdx == -1)
                currentRouteDist = 0;
            else
                currentRouteDist += fastDist(prevCustIdx, custIdx);

            double distToOffice = fastDist(custIdx, OFFICE_IDX);

            double bestTripCost = INF;
            int bestVehicle = -1;
            double bestFinishTime = -1.0;

            for (int k = 0; k < M; k++)
            {
                const Driver &d = drivers[k];

                if (d.capacity < currentLoad)
                    continue;

                bool prefFail = false;
                for (int p = i; p < j; p++)
                {
                    if (persons[chromo.giantTour[p]].pref_vehicle == PREMIUM && d.category != PREMIUM)
                    {
                        prefFail = true;
                        break;
                    }
                }
                if (prefFail)
                    continue;

                int firstCust = chromo.giantTour[i];
                double availTime = V[i].driverAvailTimes[k];

                int vehIdxMatrix = MAT_N + k;
                double distStartToFirst = (availTime == d.start_time)
                                              ? fastDist(vehIdxMatrix, firstCust)
                                              : fastDist(OFFICE_IDX, firstCust);

                double totalDist = distStartToFirst + currentRouteDist + distToOffice;

                double timeToFirst = (availTime == d.start_time)
                                         ? fastTime(vehIdxMatrix, firstCust, d.speed_kmph)
                                         : fastTime(OFFICE_IDX, firstCust, d.speed_kmph);

                double arrivalAtNode = availTime + timeToFirst;
                double currentVisTime = max(arrivalAtNode, (double)persons[firstCust].early_pickup);

                vector<double> pickupTimes;
                pickupTimes.push_back(currentVisTime);

                int tempPrev = firstCust;

                for (int p = i + 1; p < j; p++)
                {
                    int pIdx = chromo.giantTour[p];
                    double legTime = fastTime(tempPrev, pIdx, d.speed_kmph);
                    double arrival = currentVisTime + legTime;
                    double startSrv = max(arrival, (double)persons[pIdx].early_pickup);

                    pickupTimes.push_back(startSrv);
                    currentVisTime = startSrv;
                    tempPrev = pIdx;
                }

                double travelToOffice = fastTime(tempPrev, OFFICE_IDX, d.speed_kmph);
                double arrivalAtOffice = currentVisTime + travelToOffice;

                double penalty = 0.0;

                for (int p = i; p < j; p++)
                {
                    int pIdx = chromo.giantTour[p];
                    int idxInGroup = p - i;
                    double actualPickup = pickupTimes[idxInGroup];

                    if (arrivalAtOffice > persons[pIdx].late_drop)
                    {
                        double violation = arrivalAtOffice - persons[pIdx].late_drop;
                        penalty += INFEASIBILITY_PENALTY * violation;
                    }

                    double actualRideTime = arrivalAtOffice - actualPickup;
                    double directTime = fastTime(pIdx, OFFICE_IDX, d.speed_kmph);
                    double delay = actualRideTime - directTime;
                    int allowedDelay = getPriorityDelay(persons[pIdx].priority);

                    if (delay > allowedDelay)
                    {
                        double violation = delay - allowedDelay;
                        penalty += INFEASIBILITY_PENALTY * violation;
                    }
                }

                double totalPassengerRideTime = 0;
                for (int p = i; p < j; p++)
                {
                    int idxInGroup = p - i;
                    double actualPickup = pickupTimes[idxInGroup];
                    totalPassengerRideTime += (arrivalAtOffice - actualPickup);
                }

                double monetaryCost = totalDist * d.cost_per_km;
                double cost = (monetaryCost * OBJ_COST_WEIGHT) + (totalPassengerRideTime * OBJ_TIME_WEIGHT) + penalty;

                if (cost < bestTripCost)
                {
                    bestTripCost = cost;
                    bestVehicle = k;
                    bestFinishTime = arrivalAtOffice;
                }
            }

            if (bestVehicle != -1)
            {
                double newCost = V[i].cost + bestTripCost;
                if (newCost < V[j].cost)
                {
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
    if (chromo.fitness >= INF)
        return;

    chromo.schedule.clear(); // Clear out to prep for new schedule
    int curr = N;
    map<int, bool> usedVehicles;

    while (curr > 0)
    {
        int prev = V[curr].pred;
        int vIdx = V[curr].vehicleIdx;

        Chromosome::Trip trip;
        trip.vehicleIdx = vIdx;
        trip.finishTime = V[curr].finishTime;

        for (int k = prev; k < curr; k++)
            trip.customers.push_back(chromo.giantTour[k]);

        chromo.schedule.push_back(trip);
        usedVehicles[vIdx] = true;
        curr = prev;
    }

    chromo.numVehiclesUsed = usedVehicles.size();
    reverse(chromo.schedule.begin(), chromo.schedule.end());
}

// =================== GENETIC OPERATORS ===================

Chromosome crossover(const Chromosome &p1, const Chromosome &p2, mt19937 &rng)
{
    int n = p1.giantTour.size();
    Chromosome child(n);
    vector<bool> present(n, false);

    int start = rng() % n;
    int end = rng() % n;
    if (start > end)
        swap(start, end);

    for (int i = start; i <= end; i++)
    {
        child.giantTour[i] = p1.giantTour[i];
        present[p1.giantTour[i]] = true;
    }

    int curr = (end + 1) % n;
    int p2_idx = (end + 1) % n;

    while (curr != start)
    {
        int gene = p2.giantTour[p2_idx];
        if (!present[gene])
        {
            child.giantTour[curr] = gene;
            curr = (curr + 1) % n;
        }
        p2_idx = (p2_idx + 1) % n;
    }
    return child;
}

void mutate(Chromosome &c, mt19937 &rng)
{
    int n = c.giantTour.size();
    int i = rng() % n;
    int j = rng() % n;
    swap(c.giantTour[i], c.giantTour[j]);
}

// =================== DETAILED METRICS CALCULATION ===================

struct SolutionMetrics
{
    double totalDistance;
    double totalDuration;
    double totalPassengerRideTime;
    double monetaryCost;
    double weightedObjective;
};

SolutionMetrics calculateDetailedMetrics(const Chromosome &best,
                                         const vector<Person> &persons,
                                         const vector<Driver> &drivers)
{
    SolutionMetrics metrics = {0, 0, 0, 0, 0};
    int N = persons.size();

    map<int, vector<Chromosome::Trip>> driverTrips;
    for (const auto &t : best.schedule)
        driverTrips[t.vehicleIdx].push_back(t);

    for (auto &dt : driverTrips)
    {
        int vIdx = dt.first;
        const Driver &d = drivers[vIdx];

        double vehicleStartTime = d.start_time;
        double currentTime = vehicleStartTime;

        for (int i = 0; i < (int)dt.second.size(); i++)
        {
            auto &trip = dt.second[i];
            int firstCust = trip.customers[0];
            int vehIdxMatrix = MAT_N + vIdx;

            double legDist = (i == 0) ? fastDist(vehIdxMatrix, firstCust) : fastDist(OFFICE_IDX, firstCust);
            double travelTime = (i == 0) ? fastTime(vehIdxMatrix, firstCust, d.speed_kmph) : fastTime(OFFICE_IDX, firstCust, d.speed_kmph);

            currentTime = max(currentTime + travelTime, (double)persons[firstCust].early_pickup);
            metrics.totalDistance += legDist;

            int prevCust = firstCust;
            vector<double> pickupTimes;
            pickupTimes.push_back(currentTime);

            for (size_t k = 1; k < trip.customers.size(); k++)
            {
                int cIdx = trip.customers[k];
                double dist = fastDist(prevCust, cIdx);
                double travel = fastTime(prevCust, cIdx, d.speed_kmph);
                currentTime = max(currentTime + travel, (double)persons[cIdx].early_pickup);

                pickupTimes.push_back(currentTime);
                metrics.totalDistance += dist;
                prevCust = cIdx;
            }

            double toOffice = fastDist(prevCust, OFFICE_IDX);
            double travelOffice = fastTime(prevCust, OFFICE_IDX, d.speed_kmph);
            double arrivalAtOffice = currentTime + travelOffice;
            metrics.totalDistance += toOffice;

            for (size_t k = 0; k < trip.customers.size(); k++)
                metrics.totalPassengerRideTime += (arrivalAtOffice - pickupTimes[k]);

            currentTime = arrivalAtOffice;
        }

        metrics.totalDuration += (currentTime - vehicleStartTime);
    }

    metrics.monetaryCost = 0;
    for (auto &dt : driverTrips)
    {
        int vIdx = dt.first;
        const Driver &d = drivers[vIdx];
        double vehDist = 0;
        int vehIdxMatrix = MAT_N + vIdx;

        for (int i = 0; i < (int)dt.second.size(); i++)
        {
            auto &trip = dt.second[i];
            int firstCust = trip.customers[0];
            double legDist = (i == 0) ? fastDist(vehIdxMatrix, firstCust) : fastDist(OFFICE_IDX, firstCust);
            vehDist += legDist;

            int prevCust = firstCust;
            for (size_t k = 1; k < trip.customers.size(); k++)
            {
                int cIdx = trip.customers[k];
                vehDist += fastDist(prevCust, cIdx);
                prevCust = cIdx;
            }
            vehDist += fastDist(prevCust, OFFICE_IDX);
        }
        metrics.monetaryCost += vehDist * d.cost_per_km;
    }

    metrics.weightedObjective = (metrics.monetaryCost * OBJ_COST_WEIGHT) +
                                (metrics.totalPassengerRideTime * OBJ_TIME_WEIGHT);
    return metrics;
}

// =================== CSV READING FUNCTIONS ===================

vector<Driver> readVehicleCSV(const string &filename)
{
    vector<Driver> drivers;
    ifstream file(filename);
    if (!file.is_open())
        return drivers;

    string line;
    getline(file, line);

    int id = 0;
    while (getline(file, line))
    {
        stringstream ss(line);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        string vehicle_id, fuel_type, vehicle_mode, seating_str, cost_str, speed_str,
            loc_x_str, loc_y_str, avail_time_str, category_str;

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
    return drivers;
}

vector<Person> readEmployeeCSV(const string &filename)
{
    vector<Person> persons;
    ifstream file(filename);
    if (!file.is_open())
        return persons;

    string line;
    getline(file, line);

    int id = 0;
    while (getline(file, line))
    {
        stringstream ss(line);
        string emp_id, priority_str, pickup_x_str, pickup_y_str, dest_x_str, dest_y_str,
            time_start_str, time_end_str, veh_pref_str, share_pref_str;

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
    return persons;
}

map<string, int> buildEmployeeIdMap(const vector<Person> &persons)
{
    map<string, int> empMap;
    for (const auto &p : persons)
        empMap[p.original_id] = p.id;
    return empMap;
}

map<string, int> buildVehicleIdMap(const vector<Driver> &drivers)
{
    map<string, int> vehMap;
    for (const auto &d : drivers)
        vehMap[d.original_id] = d.id;
    return vehMap;
}

// NEW: This now physically groups employees by their assigned vehicle
// and sorts them by pickup time to perfectly recreate the exact trips.
Chromosome readCSVSolution(const string &filename, const vector<Person> &persons, const vector<Driver> &drivers)
{
    Chromosome chromo(persons.size());
    map<string, int> empMap = buildEmployeeIdMap(persons);
    map<string, int> vehMap = buildVehicleIdMap(drivers);

    ifstream file(filename);
    if (!file.is_open())
        return chromo;

    string line;
    getline(file, line);

    struct TempNode
    {
        int empIdx;
        double pickupTime;
    };
    map<int, vector<TempNode>> vehAssignments;
    set<int> seen;

    while (getline(file, line))
    {
        stringstream ss(line);
        string vehicle_id, category, employee_id, pickup_time, drop_time;

        getline(ss, vehicle_id, ',');
        getline(ss, category, ',');
        getline(ss, employee_id, ',');
        getline(ss, pickup_time, ',');
        getline(ss, drop_time, ',');

        employee_id = trim(employee_id);
        vehicle_id = trim(vehicle_id);

        if (empMap.count(employee_id) && vehMap.count(vehicle_id))
        {
            int empIdx = empMap[employee_id];
            int vehIdx = vehMap[vehicle_id];
            double pTime = (double)timeToMinutes(trim(pickup_time));

            vehAssignments[vehIdx].push_back({empIdx, pTime});
            seen.insert(empIdx);
        }
    }
    file.close();

    vector<int> tour;
    chromo.schedule.clear();

    for (auto &pair : vehAssignments)
    {
        int vIdx = pair.first;
        auto &nodes = pair.second;

        // Ensure they are processed in the exact order they were picked up
        sort(nodes.begin(), nodes.end(), [](const TempNode &a, const TempNode &b)
             { return a.pickupTime < b.pickupTime; });

        Chromosome::Trip trip;
        trip.vehicleIdx = vIdx;
        for (auto &n : nodes)
        {
            trip.customers.push_back(n.empIdx);
            tour.push_back(n.empIdx);
        }
        chromo.schedule.push_back(trip);
    }

    for (const auto &p : persons)
    {
        if (seen.find(p.id) == seen.end())
        {
            tour.push_back(p.id);
        }
    }

    chromo.giantTour = tour;
    chromo.numVehiclesUsed = chromo.schedule.size();
    return chromo;
}

// =================== CSV OUTPUT FUNCTIONS ===================

void writeCSVOutput(const string &filename,
                    const Chromosome &solution,
                    const vector<Person> &persons,
                    const vector<Driver> &drivers)
{
    ofstream file(filename);
    if (!file.is_open())
        return;

    file << "vehicle_id,category,employee_id,pickup_time,drop_time\n";

    map<int, vector<Chromosome::Trip>> driverTrips;
    for (const auto &t : solution.schedule)
        driverTrips[t.vehicleIdx].push_back(t);

    for (auto &dt : driverTrips)
    {
        int vIdx = dt.first;
        const Driver &d = drivers[vIdx];
        double currentTime = d.start_time;
        int vehIdxMatrix = MAT_N + vIdx;

        for (int i = 0; i < (int)dt.second.size(); i++)
        {
            auto &trip = dt.second[i];
            int firstCust = trip.customers[0];

            double travelTime = (i == 0) ? fastTime(vehIdxMatrix, firstCust, d.speed_kmph) : fastTime(OFFICE_IDX, firstCust, d.speed_kmph);
            currentTime = max(currentTime + travelTime, (double)persons[firstCust].early_pickup);

            vector<double> pickupTimes;
            pickupTimes.push_back(currentTime);
            int prevCust = firstCust;

            for (size_t k = 1; k < trip.customers.size(); k++)
            {
                int cIdx = trip.customers[k];
                double travel = fastTime(prevCust, cIdx, d.speed_kmph);
                currentTime = max(currentTime + travel, (double)persons[cIdx].early_pickup);

                pickupTimes.push_back(currentTime);
                prevCust = cIdx;
            }

            double travelOffice = fastTime(prevCust, OFFICE_IDX, d.speed_kmph);
            double arrivalAtOffice = currentTime + travelOffice;

            for (size_t k = 0; k < trip.customers.size(); k++)
            {
                int empIdx = trip.customers[k];
                file << d.original_id << ","
                     << (d.category == PREMIUM ? "Premium" : "Normal") << ","
                     << persons[empIdx].original_id << ","
                     << minToTime(pickupTimes[k]) << ","
                     << minToTime(arrivalAtOffice) << "\n";
            }
            currentTime = arrivalAtOffice;
        }
    }
    file.close();
}

// =================== MAIN DRIVER ===================

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " <test_case_folder_path>" << endl;
        return 1;
    }

    string basePath = argv[1];
    if (basePath.back() != '/')
        basePath += '/';

    readMetadata(basePath + "metadata.csv");

    vector<Driver> drivers = readVehicleCSV(basePath + "vehicles.csv");
    vector<Person> persons = readEmployeeCSV(basePath + "employees.csv");

    if (drivers.empty() || persons.empty())
        return 1;

    MAT_N = (int)persons.size();
    MAT_V = (int)drivers.size();
    int matSize = MAT_N + MAT_V + 1;
    OFFICE_IDX = MAT_N + MAT_V;

    loadMatrix(basePath + "matrix.txt", matSize);

    vector<string> subfolders = {
        "ALNS",
        "Branch-And-Cut",
        "Clustering-Routing-DP-Solver",
        "Heterogeneous_DARP",
        "Variable_Neighbourhood_Search",
        "god"};

    cout << "[INFO] Reading initial population from algorithm subfolder outputs..." << endl;
    vector<Chromosome> population;

    for (const auto &folder : subfolders)
    {
        string filepath = basePath + folder + "/output_vehicle.csv";
        ifstream testFile(filepath);
        if (!testFile.is_open())
            continue;
        testFile.close();

        // NEW: Load the exact schedule and manually calculate its real cost.
        Chromosome c = readCSVSolution(filepath, persons, drivers);
        c.fitness = evaluateExactSchedule(c, persons, drivers);

        population.push_back(c);
        cout << "  - Loaded " << folder << " | Exact CSV Fitness: " << fixed << setprecision(2) << c.fitness << endl;
    }

    if (population.empty())
    {
        cerr << "Error: No initial solutions could be loaded!" << endl;
        return 1;
    }

    int POP_SIZE = max(50, (int)population.size() * 5);
    int GENERATIONS = 200;
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

    auto start_time = chrono::high_resolution_clock::now();

    for (int gen = 0; gen < GENERATIONS; gen++)
    {
        sort(population.begin(), population.end(),
             [](const Chromosome &a, const Chromosome &b)
             { return a.fitness < b.fitness; });

        vector<Chromosome> nextPop;
        nextPop.push_back(population[0]); // Elitism

        while ((int)nextPop.size() < POP_SIZE)
        {
            int t1 = rng() % population.size();
            int t2 = rng() % population.size();
            Chromosome child = crossover(population[t1], population[t2], rng);
            if (rng() % 100 < 15)
                mutate(child, rng);

            // Generate the new optimal vehicle routes for the mutated child sequence
            splitProcedure(child, persons, drivers);
            nextPop.push_back(child);
        }
        population = nextPop;
    }

    auto end_time = chrono::high_resolution_clock::now();
    double duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count() / 1000.0;

    sort(population.begin(), population.end(),
         [](const Chromosome &a, const Chromosome &b)
         { return a.fitness < b.fitness; });
    Chromosome best = population[0];

    cout << "\n[INFO] Writing final solution to CSV..." << endl;
    writeCSVOutput(basePath + "memetic_algorithm/final_output_vehicle.csv", best, persons, drivers);

    cout << "\n"
         << string(80, '=') << "\n";
    cout << "                    FINAL METRICS BREAKDOWN\n";
    cout << string(80, '=') << "\n\n";

    if (best.fitness >= INF)
    {
        cout << "NO FEASIBLE SOLUTION FOUND!" << endl;
    }
    else
    {
        SolutionMetrics metrics = calculateDetailedMetrics(best, persons, drivers);

        cout << "Total Distance:                    " << fixed << setprecision(2) << metrics.totalDistance << " km\n";
        cout << "Total Cost:                        " << metrics.monetaryCost << "\n";
        cout << "Total Passenger Ride Time:         " << metrics.totalPassengerRideTime << " min\n\n";
        cout << "WEIGHTED OBJECTIVE FUNCTION:       " << metrics.weightedObjective << "\n";
    }

    return 0;
}