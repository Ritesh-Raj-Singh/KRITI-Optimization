# Clustering-Routing-DP-Solver

## 1. Project Overview
This project implements a high-performance solver for the **Capacitated Vehicle Routing Problem with Time Windows (CVRPTW)** in the context of corporate employee transport. Unlike standard VRP solvers that focus solely on distance, this system uses a **Multi-Objective Cost Function** to balance operational costs (money) against passenger convenience (time).

The solution is designed to be **Metadata-Driven**, allowing administrators to tune optimization goals and priority rules via configuration files without recompiling the code.

## 2. Key Features

### A. Dynamic Configuration (Metadata)
The system behavior is controlled by `metadata{tc}.csv`.
* **Objective Weights:** Users can define the ratio of importance between Cost and Time.
    * `objective_cost_weight`: Weight for monetary cost (e.g., 0.7 for 70%).
    * `objective_time_weight`: Weight for passenger wait/travel time (e.g., 0.3 for 30%).
* **Priority Rules:** Deadline extensions are dynamically loaded for each priority level (e.g., `priority_1_max_delay_min` = 5 mins).

### B. Weighted Optimization Engine
The routing engine calculates a **Weighted Score** for every potential route:
$$Z = (W_{cost} \times \text{TripCost}) + (W_{time} \times \text{TotalPassengerTime})$$
* **TripCost:** (Total Distance $\times$ Vehicle Cost per KM).
* **TotalPassengerTime:** Sum of (Arrival Time at Office - Earliest Pickup Time) for all passengers in the vehicle.

This ensures that a cheaper vehicle is not selected if it causes excessive delays for passengers.

### C. Advanced Clustering & Fallback Logic
1.  **Spatiotemporal Clustering:** Employees are grouped based on both geographic proximity and schedule compatibility. A **Time Penalty** is applied to prevent grouping employees with disparate shifts.
2.  **Smart Fallback Strategy:** If a formed cluster cannot fit into any available vehicle (due to capacity or deadlines), the system:
    * Attempts to split the cluster into two smaller sub-groups.
    * Recursively attempts assignment.
    * Falls back to individual assignments only as a last resort.

## 3. Project Structure

| File | Description |
| :--- | :--- |
| `main.cpp` | The complete C++ source code containing Clustering, Routing, and I/O logic. |
| `Employee{tc}.csv` | Input data: Employee locations, pickup times, priorities, and sharing preferences. |
| `Vehicles{tc}.csv` | Input data: Fleet specifications (Capacity, Speed, Cost/KM, Availability). |
| `metadata{tc}.csv` | Input data: Global configuration weights and priority rules. |
| `output_vehicle{tc}.csv` | Output: Detailed trip logs (Vehicle ID, Pickup/Drop times). |
| `output_employee{tc}.csv` | Output: Employee-specific schedule manifest. |

## 4. Input Specifications

### Metadata File (`metadata{tc}.csv`)
Must follow the `key,value` format. Example:
```csv
key,value
objective_cost_weight,0.6
objective_time_weight,0.4
priority_1_max_delay_min,5
priority_2_max_delay_min,10
priority_5_max_delay_min,30
