#include "io.h"
#include <cstdio>
#include <algorithm>

int id_from_string(const std::string &vehicle_id_str)
{
    if (vehicle_id_str.size() < 2 || !std::isdigit(vehicle_id_str[1]))
    {
        throw std::invalid_argument("Invalid vehicle ID format");
    }

    return std::stoi(vehicle_id_str.substr(1));
}

std::map<int, int> PRIORITY_DELAYS;
double WEIGHT_COST = 0.7;
double WEIGHT_TIME = 0.3;

int read_vehicle_data(std::string file_name, DARPInstance &instance)
{
    int number_of_vehicles = 0;
    std::ifstream file(file_name);
    std::string line;

    if (!file.is_open())
    {
        std::cerr << "Could not open " << file_name << " file\n";
        return 0;
    }

    // Read header line
    std::getline(file, line);

    // Read data lines
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::stringstream ss(line);
        std::string value;

        std::string vehicle_id_str, fuel_type_str, vehicle_type, capacity_str,
            cost_per_km_str, average_speed_kmph_str, curr_lat_str,
            curr_lon_str, available_from_time_str, category_str;

        std::getline(ss, vehicle_id_str, ',');
        std::getline(ss, fuel_type_str, ',');
        std::getline(ss, vehicle_type, ',');
        std::getline(ss, capacity_str, ',');
        std::getline(ss, cost_per_km_str, ',');
        std::getline(ss, average_speed_kmph_str, ',');
        std::getline(ss, curr_lat_str, ',');
        std::getline(ss, curr_lon_str, ',');
        std::getline(ss, available_from_time_str, ',');
        std::getline(ss, category_str, ',');

        int vehicle_id = id_from_string(vehicle_id_str);
        int capacity = std::stoi(capacity_str);
        double cost_per_km = std::stod(cost_per_km_str);
        double average_speed_kmph = std::stod(average_speed_kmph_str);
        double curr_lat = std::stod(curr_lat_str);
        double curr_lon = std::stod(curr_lon_str);

        int category;
        if (category_str == "premium")
            category = 0; // 0 : premium
        else
            category = 1; // 1 : normal

        // Parse time
        double available_from_time;
        std::stringstream time_ss(available_from_time_str);
        std::string hrs_str, mins_str;
        std::getline(time_ss, hrs_str, ':');
        std::getline(time_ss, mins_str, ':');
        int hrs = std::stoi(hrs_str);
        int mins = std::stoi(mins_str);
        available_from_time = hrs * 60 + mins; // Convert to minutes

        // Process the vehicle data
        Node depot_node(
            vehicle_id, NodeType::DEPOT,
            curr_lat, curr_lon,
            available_from_time, 24 * 60, // Convert 24 hours to minutes
            0,
            0,
            0);
        instance.set_node(vehicle_id, depot_node);

        Vehicle v(
            vehicle_id,
            vehicle_id,
            capacity,
            0,
            cost_per_km,
            0,
            0,
            0,
            category,
            average_speed_kmph);

        instance.vehicles.push_back(v);

        number_of_vehicles++;
    }

    file.close();
    return number_of_vehicles;
}

int read_employee_data(std::string file_name, DARPInstance &instance)
{
    int number_of_employees = 0;
    std::ifstream file(file_name);
    std::string line;

    if (!file.is_open())
    {
        std::cerr << "Could not open " << file_name << " file\n";
        return 0;
    }

    // Read header line
    std::getline(file, line);

    // Read data lines
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::stringstream ss(line);
        std::string value;

        std::string employee_id_str, priority_str, pickup_lat_str, pickup_lon_str,
            drop_lat_str, drop_lon_str, earliest_pickup_str, latest_drop_str,
            vehicle_preference_str, sharing_preference_str;

        std::getline(ss, employee_id_str, ',');
        std::getline(ss, priority_str, ',');
        std::getline(ss, pickup_lat_str, ',');
        std::getline(ss, pickup_lon_str, ',');
        std::getline(ss, drop_lat_str, ',');
        std::getline(ss, drop_lon_str, ',');
        std::getline(ss, earliest_pickup_str, ',');
        std::getline(ss, latest_drop_str, ',');
        std::getline(ss, vehicle_preference_str, ',');
        std::getline(ss, sharing_preference_str, ',');

        int employee_id = id_from_string(employee_id_str);
        int priority = std::stoi(priority_str);
        double pickup_lat = std::stod(pickup_lat_str);
        double pickup_lon = std::stod(pickup_lon_str);
        double drop_lat = std::stod(drop_lat_str);
        double drop_lon = std::stod(drop_lon_str);

        int vehicle_preference;
        if (vehicle_preference_str == "premium")
            vehicle_preference = 0; // 0 : premium
        else
            vehicle_preference = 1; // 1 : normal, any

        int sharing_preference;
        if (sharing_preference_str == "single")
            sharing_preference = 1;
        else if (sharing_preference_str == "double")
            sharing_preference = 2;
        else if (sharing_preference_str == "triple")
            sharing_preference = 3;
        else
            sharing_preference = 3; // Default to triple

        // Parse earliest pickup time
        double earliest_pickup;
        std::stringstream time_ss1(earliest_pickup_str);
        std::string hrs_str1, mins_str1;
        std::getline(time_ss1, hrs_str1, ':');
        std::getline(time_ss1, mins_str1, ':');
        int hrs1 = std::stoi(hrs_str1);
        int mins1 = std::stoi(mins_str1);
        earliest_pickup = hrs1 * 60 + mins1; // Convert to minutes

        // Parse latest drop time
        double latest_drop;
        std::stringstream time_ss2(latest_drop_str);
        std::string hrs_str2, mins_str2;
        std::getline(time_ss2, hrs_str2, ':');
        std::getline(time_ss2, mins_str2, ':');
        int hrs2 = std::stoi(hrs_str2);
        int mins2 = std::stoi(mins_str2);
        latest_drop = hrs2 * 60 + mins2; // Convert to minutes

        int pid = 100 + employee_id;
        int did = 200 + employee_id;

        Node pickup_node(
            pid, NodeType::PICKUP,
            pickup_lat, pickup_lon,
            earliest_pickup, 24 * 60, // Convert to minutes
            0,
            1,
            sharing_preference, '\0', priority);

        int allowed_delay = 0;
        if (PRIORITY_DELAYS.count(priority) > 0)
            allowed_delay = PRIORITY_DELAYS[priority];

        Node drop_node(
            did, NodeType::DELIVERY,
            drop_lat, drop_lon,
            0, latest_drop + allowed_delay, // Allow delay based on priority
            0,
            -1,
            sharing_preference, '\0', priority);

        instance.set_node(pid, pickup_node);
        instance.set_node(did, drop_node);

        uint32_t compatible_vehicle_types = 0;
        if (vehicle_preference == 0)
            compatible_vehicle_types = (1u << 0); // Only premium
        else
            compatible_vehicle_types = (1u << 0) | (1u << 1); // Both premium and normal

        Request r(
            employee_id,
            pickup_node,
            drop_node,
            std::numeric_limits<double>::infinity(),
            compatible_vehicle_types);

        instance.register_request(r);

        number_of_employees++;
    }

    // Initialize tariffs (if not already set by metadata)
    if (instance.tariff_a.empty())
    {
        instance.tariff_a = {{60, 0.0}, {120, 0.0}, {180, 0.0}};
    }
    if (instance.tariff_b.empty())
    {
        instance.tariff_b = {{60, 0.0}, {120, 0.0}, {180, 0.0}};
    }

    file.close();
    return number_of_employees;
}

std::vector<std::string> split(const std::string &s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        if (!token.empty() && token.back() == '\r')
            token.pop_back(); // Handle Windows line endings
        tokens.push_back(token);
    }
    return tokens;
}

void read_metadata(std::string file_name)
{
    std::ifstream file(file_name);
    if (!file.is_open())
    {
        std::cerr << "Warning: Could not open " << file_name << " file. Using defaults.\n";
        return;
    }

    std::string line;
    // Skip header
    std::getline(file, line);

    // Read metadata key-value pairs
    while (std::getline(file, line))
    {
        std::vector<std::string> row = split(line, ',');
        if (row.size() < 2)
            continue;
        std::string key = row[0];
        std::string val = row[1];

        if (key == "objective_cost_weight")
            WEIGHT_COST = stod(val);
        else if (key == "objective_time_weight")
            WEIGHT_TIME = stod(val);
        else if (key.find("priority_") != std::string::npos && key.find("_max_delay_min") != std::string::npos)
        {
            int p = stoi(key.substr(9, 1));
            PRIORITY_DELAYS[p] = stoi(val);
        }
    }
    file.close();
}

void loadMatrix(const std::string &filename, DARPInstance &instance, int num_employees, int num_vehicles)
{
    const int MATRIX_SIZE = num_employees + num_vehicles + 1; // Actual matrix file size
    const int NUM_EMPLOYEES = num_employees;
    const int NUM_VEHICLES = num_vehicles;
    const int OFFICE_IDX = NUM_EMPLOYEES + NUM_VEHICLES; // Last matrix index is office

    // Create mapping from matrix index to VNS node ID
    std::vector<int> matrix_to_node_id(MATRIX_SIZE);

    // Matrix indices 0..(NUM_EMPLOYEES-1) (employee pickups) → Node IDs 101..100+N
    for (int i = 0; i < NUM_EMPLOYEES; i++)
    {
        matrix_to_node_id[i] = 101 + i;
    }

    // Matrix indices NUM_EMPLOYEES..(NUM_EMPLOYEES+NUM_VEHICLES-1) (vehicle depots) → Node IDs 1..NUM_VEHICLES
    for (int i = 0; i < NUM_VEHICLES; i++)
    {
        matrix_to_node_id[NUM_EMPLOYEES + i] = 1 + i;
    }

    // Office index - we'll handle this separately for deliveries
    matrix_to_node_id[OFFICE_IDX] = 201; // Temporary mapping

    // Find maximum node ID needed
    int max_node_id = 200 + NUM_EMPLOYEES; // Highest delivery node
    instance.fit_structures(max_node_id);

    // Initialize all distances to infinity
    for (int i = 0; i <= max_node_id; i++)
    {
        for (int j = 0; j <= max_node_id; j++)
        {
            instance.distance_matrix[i][j] = std::numeric_limits<double>::infinity();
            instance.time_matrix[i][j] = std::numeric_limits<double>::infinity();
        }
    }

    // Read the 12x12 matrix from file
    std::ifstream fin(filename);
    if (!fin)
    {
        std::cerr << "Cannot open matrix file: " << filename << "\n";
        std::exit(1);
    }

    std::vector<std::vector<double>> temp_matrix(MATRIX_SIZE, std::vector<double>(MATRIX_SIZE));
    for (int i = 0; i < MATRIX_SIZE; i++)
    {
        for (int j = 0; j < MATRIX_SIZE; j++)
        {
            fin >> temp_matrix[i][j];
        }
    }
    fin.close();

    // Map the 12x12 matrix to VNS node ID space
    for (int i = 0; i < MATRIX_SIZE; i++)
    {
        for (int j = 0; j < MATRIX_SIZE; j++)
        {
            int node_i = matrix_to_node_id[i];
            int node_j = matrix_to_node_id[j];

            double dist = temp_matrix[i][j];

            // Handle office index specially
            if (i == OFFICE_IDX)
            {
                // From office to other nodes - map to all delivery nodes
                for (int d = 201; d <= 200 + NUM_EMPLOYEES; d++)
                {
                    instance.distance_matrix[d][node_j] = dist;
                    instance.time_matrix[d][node_j] = dist;
                }
            }
            if (j == OFFICE_IDX)
            {
                // From other nodes to office - map to all delivery nodes
                for (int d = 201; d <= 200 + NUM_EMPLOYEES; d++)
                {
                    instance.distance_matrix[node_i][d] = dist;
                    instance.time_matrix[node_i][d] = dist;
                }
            }
            if (i != OFFICE_IDX && j != OFFICE_IDX)
            {
                // Regular mapping
                instance.distance_matrix[node_i][node_j] = dist;
                instance.time_matrix[node_i][node_j] = dist;
            }
        }
    }

    // Special case: office to office (all delivery nodes to each other)
    double office_to_office = temp_matrix[OFFICE_IDX][OFFICE_IDX];
    for (int d1 = 201; d1 <= 200 + NUM_EMPLOYEES; d1++)
    {
        for (int d2 = 201; d2 <= 200 + NUM_EMPLOYEES; d2++)
        {
            instance.distance_matrix[d1][d2] = office_to_office;
            instance.time_matrix[d1][d2] = office_to_office;
        }
    }

    std::cout << "Matrix loaded and remapped: " << MATRIX_SIZE << "x" << MATRIX_SIZE
              << " → " << (max_node_id + 1) << "x" << (max_node_id + 1) << " (sparse)\n";
    std::cout << "Mapping:\n";
    std::cout << "  Matrix [0-" << (NUM_EMPLOYEES - 1) << "]  (Employee Pickups) → Nodes [101-" << (100 + NUM_EMPLOYEES) << "]\n";
    std::cout << "  Matrix [" << NUM_EMPLOYEES << "-" << (NUM_EMPLOYEES + NUM_VEHICLES - 1) << "] (Vehicle Depots)   → Nodes [1-" << NUM_VEHICLES << "]\n";
    std::cout << "  Matrix [" << OFFICE_IDX << "]   (Office)           → Nodes [201-" << (200 + NUM_EMPLOYEES) << "]\n";
}

// Helper: convert minutes since midnight to "HH:MM" string
static std::string min_to_time_str(int minutes)
{
    int hrs = minutes / 60;
    int mins = minutes % 60;
    char buf[6];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", hrs, mins);
    return std::string(buf);
}

// Helper: format an integer ID with a prefix and zero-padding (e.g., 'V', 1 → "V01")
static std::string format_id(char prefix, int id)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%c%02d", prefix, id);
    return std::string(buf);
}

// Writes output_vehicles.csv and output_employees.csv in the current directory.
// Simulates route timing (matching evaluate.cpp logic) to extract pickup/drop times.
//
// output_vehicles.csv format:
//   vehicle_id,category,employee_id,pickup_time,drop_time
//
// output_employees.csv format:
//   employee_id,pickup_time,drop_time
//
void write_output_csvs(const Solution &solution, DARPInstance &instance, std::string base)
{
    struct CSVRow
    {
        int vehicle_id;
        std::string category;
        int employee_id;
        std::string pickup_time;
        std::string drop_time;
    };

    std::vector<CSVRow> rows;

    for (const Route &route : solution.routes)
    {
        if (route.sequence.empty())
            continue;

        const Vehicle &vehicle = *route.vehicle;
        double speed = vehicle.average_speed;
        std::string category = (vehicle.type_id == 0) ? "Premium" : "Normal";

        // Timing simulation (mirrors calculate_route_stats in evaluate.cpp)
        int prev_node_id = vehicle.depot_id;
        const Node *prev_node = &instance.nodes[prev_node_id];

        const Node &first_node = instance.nodes[route.sequence.front()];
        double dist_to_first = instance.get_dist(prev_node_id, first_node.id);
        const Node &depot_node = instance.nodes[vehicle.depot_id];
        double start_time = std::max(depot_node.e, first_node.e - (dist_to_first / speed) * 60.0);
        double current_time = start_time;

        // Map request_id -> pickup time string
        std::unordered_map<int, std::string> pickup_time_of_request;

        for (int node_id : route.sequence)
        {
            const Node &node = instance.nodes[node_id];

            double dist = instance.get_dist(prev_node_id, node.id);
            double travel_time = (dist / speed) * 60.0;
            double arrival_time = current_time + prev_node->st + travel_time;
            double start_service = std::max(node.e, arrival_time);

            if (node.type == NodeType::PICKUP)
            {
                int req_id = instance.get_req_id_by_node(node.id);
                pickup_time_of_request[req_id] = min_to_time_str(static_cast<int>(start_service));
            }
            else if (node.type == NodeType::DELIVERY)
            {
                int req_id = instance.get_req_id_by_node(node.id);
                auto it = pickup_time_of_request.find(req_id);
                if (it != pickup_time_of_request.end())
                {
                    CSVRow row;
                    row.vehicle_id = vehicle.id;
                    row.category = category;
                    row.employee_id = req_id;
                    row.pickup_time = it->second;
                    row.drop_time = min_to_time_str(static_cast<int>(start_service));
                    rows.push_back(row);
                    pickup_time_of_request.erase(it);
                }
            }

            current_time = start_service;
            prev_node = &node;
            prev_node_id = node.id;
        }
    }

    // Write output_vehicles.csv
    // Format: vehicle_id,category,employee_id,pickup_time,drop_time
    {
        std::ofstream fout(base + "/god/output_vehicle.csv", std::ios::trunc);
        fout << "vehicle_id,category,employee_id,pickup_time,drop_time\n";
        for (const CSVRow &r : rows)
            fout << format_id('V', r.vehicle_id) << "," << r.category << "," << format_id('E', r.employee_id)
                 << "," << r.pickup_time << "," << r.drop_time << "\n";
        fout.close();
    }

    // Write output_employees.csv
    // Format: employee_id,pickup_time,drop_time
    {
        std::ofstream fout(base + "/god/output_employees.csv", std::ios::trunc);
        fout << "employee_id,pickup_time,drop_time\n";
        for (const CSVRow &r : rows)
            fout << format_id('E', r.employee_id) << "," << r.pickup_time << "," << r.drop_time << "\n";
        fout.close();
    }

    std::cout << "\nCSV Files Generated: output_vehicles.csv, output_employees.csv\n";
}
