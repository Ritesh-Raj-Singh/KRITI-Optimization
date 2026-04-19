# VELORA - KRITI Optimization

Welcome to the **KRITI Optimization System**, a robust full-stack application built to solve advanced routing, vehicle scheduling, and dynamic allocation problems utilizing state-of-the-art C++ heuristic algorithms combined with an interactive Web Application frontend.

---

## 🏛️ Repository Architecture

This codebase has been heavily engineered and structured according to modern C++ conventions and standard web architecture:

```text
KRITI-Optimization/
├── frontend/                 # Next.js React Web Application
├── include/
│   └── third_party/          # Header-only dependencies (crow_all.h, json.hpp)
├── src/
│   ├── algorithms/           # Isolated Routing & Optimization Solvers
│   │   ├── alns/             # Adaptive Large Neighborhood Search
│   │   ├── branch_and_cut/   # Exact Branch and Cut Logic
│   │   ├── crdp/             # Clustering-Routing-DP Solver
│   │   ├── god_vns/          # Generic Optimization & Variable Neighborhood Search
│   │   ├── heterogeneous_darp/# Heterogeneous DARP logic
│   │   ├── memetic/          # Memetic Algorithm logic
│   │   └── vns/              # Additional VNS implementations
│   └── server/               
│       └── main.cpp          # The Crow Backend API Server
├── build/
│   └── bin/                  # Centralized output registry for all compiled executables
├── Makefile                  # Root automated compilation logic
└── package.json              # Node dependencies (e.g. clang-format hooks)
```

---

## 🧠 The Engine: `main.cpp`

The backend orchestrator (`src/server/main.cpp`) acts as the synchronization and translation pipeline bridging user-supplied inputs and complex C++ solvers. The major operations configured in `main.cpp` include:

1. **Crow REST API**: It establishes a lightning-fast HTTP server to interpret massive multi-part HTTP POST data forms housing employee/vehicle CSV configurations.
2. **Ephemeral Space Generation**: For data integrity, it constructs isolated, zero-collision `/tmp/req_<UUID>` sandbox environments ensuring multiple simultaneous API requests never overlap or corrupt shared directories.
3. **Automated Distance Matrixing**: Utilizes system `curl` pipelines to dynamically interact with the **OSRM Networking Project** to compile legitimate traffic-distance calculation matrices for nodes. Automatically engages mathematical fallback `Haversine` routing locally if API networks fail.
4. **Concurrent Execution Engine**: Leveraging `std::thread`, the backend simultaneously boots execution threads mapping directly to `build/bin/main_alns`, `build/bin/main_bac`, and others. These binaries read the sandbox CSV matrices and output algorithmic payload routing solutions.
5. **JSON Aggregation**: Translates parsed algorithm CSV responses identically through the single framework and pipes structured `.json` payloads back directly to the `frontend`.

---

## 🚀 How to Run the Application

The project requires both a C++ building infrastructure and a Node infrastructure.

### Prerequisites
Ensure your local environment contains the following applications:
- **`g++`** (Must support C++17)
- **`make`** (GNU Make to handle compilation)
- **`curl`** (System tool required by the C++ backend for external Matrix API fetching)
- **`node` & `npm`** (Required to run the Frontend interface)

### Step 1: Compile the Backend Algorithms
We have condensed the complexities of the various C++ solvers into a single command-line step. Run the following command exactly in your root directory:

```bash
make clean all
```
*This command mechanically sets up environment bindings, compiles the complex ALNS, Branch/Cut, VNS structures, and builds the Crow server—depositing all native executables safely in `build/bin/`.*

### Step 2: Start the Backend Server
Once compiled, you can launch the backend environment. By default, Crow exposes the primary interface on **Port 5555**.

```bash
./build/bin/server_app
```

### Step 3: Run the Web Frontend
Open a **new terminal window** and navigate into your React server directory. Boot the development frontend seamlessly:

```bash
cd frontend
npm install
npm run dev
```

You can now interact with the entire dashboard UI conventionally by navigating your browser to `http://localhost:3000`.

---

## 🛠️ Code Maintenance

This repository utilizes rigorous indentation formatting to ensure the ecosystem scales optimally.
- Use **`clang-format`** exclusively for formatting any novel changes inserted into the C++ domain using the root `.clang-format` profile.
- Variable names standardize toward `snake_case` definitions for data handling and function declarations. Class structures default strictly to `CamelCase`.
