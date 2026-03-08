# KRITI-Optimization API Backend

A high-performance, multithreaded C++ backend designed to orchestrate complex vehicle routing and employee transportation solvers. Built using the [Crow](https://crowcpp.org/) framework, this server processes batch coordinates, computes highly concurrent distance matrices using OSRM, and executes various included optimization algorithms (ALNS, Branch-and-Cut, Memetic, etc.) in isolated sandbox environments.

## System Architecture

The server operates as a central dispatcher with a 3-step pipeline for every incoming request:
1. **Data Ingestion & Sandboxing:** Parses multipart CSV data (Employees, Vehicles, Metadata) and creates an isolated, UUID-tagged temporary workspace to prevent data races between concurrent API requests.
2. **Matrix Generation:** Computes a full N x N distance matrix between all nodes (employees, vehicles, and depot) using either a highly parallelized OSRM network fetch or a Haversine fallback.
3. **Algorithm Orchestration:** Dispatches the data to the integrated C++ solver binaries, running them concurrently across multiple threads, and aggregates their CSV outputs into a unified JSON response.

## Deep Dive: Calculations & Matrix Generation

The core geographic logic is handled by the `generate_matrix_file` function, which determines the cost (distance) between any two points in the dataset.

### 1. OSRM Road Network Distances (Primary)
To get accurate real-world driving distances, the system queries a public OSRM routing engine. Because OSRM limits the number of coordinates per request (typically 100), the system implements a **Block Partitioning Algorithm**:
* The N coordinates are sliced into blocks of 99.
* The system constructs sub-matrices (e.g., Block 0 to Block 1, Block 0 to Block 2).
* Distances are returned in meters and normalized to kilometers.

### 2. Haversine Formula (Fallback / Offline Mode)
If the metadata specifies `allow_external_maps,FALSE`, or if the OSRM network request fails, the system automatically falls back to calculating the Great-Circle distance using the Haversine formula. 
* Converts latitude and longitude from degrees to radians.
* Applies the spherical distance formula using Earth's mean radius (R ≈ 6371.0 km).

## Deep Dive: Concurrency & Multithreading

This system leverages modern C++ `<thread>`, `<atomic>`, and `<mutex>` libraries to drastically reduce response times for heavy computational loads.

### Concurrent Matrix Fetching
Network I/O is the biggest bottleneck in generating large distance matrices. Instead of fetching block permutations sequentially:
* The backend spawns a `std::thread` for every `(srcBlock, dstBlock)` combination.
* A batch of `curl` requests is fired simultaneously.
* An `std::atomic<bool> osrm_failed` flag is monitored. If network errors occur, the threads are cleanly joined, and the system re-launches the thread pool entirely in Haversine mode.

### Parallel Solver Execution
Route optimization algorithms are notoriously CPU-intensive. The backend executes multiple heuristic and exact solvers in parallel:
* **Sandboxed Execution:** Each algorithm (e.g., `main_ALNS`, `main_BAC`) is executed in its own dedicated subdirectory within the request's temp folder.
* **Thread Dispatch:** A thread pool is spun up for `ALNS`, `Branch-And-Cut`, `Heterogeneous_DARP`, and `god` solvers. 
* **Thread Safety:** Standard output logging (`std::cout`) across the concurrent algorithm runners is protected by a global `std::mutex log_mutex` to prevent interleaved terminal output.
* **Sequential Processing (Solution Seeding):** The `Memetic` algorithm is executed after the parallel block joins. Rather than running from scratch, it ingests the completed outputs from the preceding algorithms as its initial population, applying evolutionary techniques to further refine and optimize the final routes.
