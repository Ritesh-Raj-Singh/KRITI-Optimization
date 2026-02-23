# 🚀 Multi-Vehicle Route Optimization System

![C++](https://img.shields.io/badge/C++-17%2B-blue.svg) ![Next.js](https://img.shields.io/badge/Next.js-14-black) ![Linux](https://img.shields.io/badge/OS-Ubuntu-orange)

This high-performance, full-stack Multi-Vehicle Employee Transportation Assignment (MVETA) solver was developed for the **Kriti Route Optimization Challenge**. It features a "Race to the Best Solution" architecture, orchestrating seven distinct optimization algorithms concurrently to minimize corporate transportation costs and employee commute times.

## ✨ Key Features

* **Multithreaded Portfolio Architecture:** A C++ backend powered by the Crow microframework dispatches 6 metaheuristic solvers simultaneously via `std::thread`, with a 7th Memetic solver that hybridizes the outputs.
* **Smart Routing Engine:** Dynamically generates an $N \times N$ distance matrix using network-aware Open Source Routing Machine (OSRM) API calls via parallel block-decomposition, with a robust Haversine fallback.
* **Penalty-Based Constraint Relaxation:** Navigates complex vehicle capacity, time-window, and sharing preferences using dynamic penalty tuning ($\alpha, \beta, \gamma, \delta$) to escape local optima.
* **Full-Stack Interactive Frontend:** A Next.js 14 web interface featuring 3D globe visualization (React-Globe.gl) and detailed 2D route mapping (Leaflet.js) to monitor fleet analytics in real-time.

## 🧠 Optimization Solvers

The system deploys seven independent executables, each tackling the routing problem from a unique mathematical approach:
1. **GOD-VNS:** Variable Neighborhood Search with penalty-steered intra-route repositioning.
2. **ALNS:** Adaptive Large Neighborhood Search with dynamic destroy/repair weights.
3. **Branch-and-Cut (BAC):** Regret-based initialization coupled with deterministic annealing.
4. **Heterogeneous DARP:** Tailored routing for mixed vehicle fleets (Normal/Premium).
5. **Clustering-Routing DP (CRDS):** A divide-and-conquer approach using divisive hierarchical clustering and Held-Karp Dynamic Programming.
6. **Standalone VNS:** Fast, constraint-aware local search with greedy best-insertion.
7. **Memetic Algorithm:** A capstone evolutionary algorithm seeded by the CSV outputs of the prior six solvers, utilizing Order Crossover (OX) on a giant-tour representation.

## 🏗️ System Architecture

* **Frontend:** Next.js 14 (App Router), React, Leaflet.js, React-Globe.gl, Framer Motion.
* **Backend:** C++, Crow HTTP Framework, nlohmann/json.
* **Inter-Process Communication:** Standardized CSV schema (Employees, Vehicles, Metadata) and UUID-isolated temporary execution directories for thread-safe operations.

## ⚙️ Prerequisites & Setup (Ubuntu/Linux)

### Backend Requirements
* **C++ Compiler:** GCC/G++ supporting C++17 or higher.
* **CMake:** Build system.
* **Libraries:** Crow (HTTP server), libcurl (for OSRM requests), nlohmann-json.

```bash
# Clone the repository
git clone [https://github.com/your-username/KRITI-Optimization.git](https://github.com/your-username/KRITI-Optimization.git)
cd KRITI-Optimization

# Build the C++ Backend and Solvers
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run the Server (Defaults to port 5555)
./server_app
