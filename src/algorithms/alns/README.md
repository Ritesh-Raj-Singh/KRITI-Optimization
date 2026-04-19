
#  VRP Runner: ALNS Project

This project implements an **Adaptive Large Neighborhood Search (ALNS)** for the Vehicle Routing Problem (VRP). It is designed to be cross-platform, allowing seamless compilation and execution on both **Windows** and **macOS/Linux**.

---

##  Prerequisites

Before running the project, ensure you have the following installed:

### Windows
* **Compiler:** `g++` (via MinGW or MSYS2)
* **Build Tool:** `mingw32-make`
    * *Note: Ensure your MinGW `bin` folder (e.g., `C:\msys64\mingw64\bin`) is added to your System PATH.*

### macOS / Linux
* **Compiler:** `g++` (supports C++17)
* **Build Tool:** `make`

---

##  Build & Run Guide

### 1. Compile the Code
Open your terminal in the `ALNS` directory and run the build command for your OS. This will create the executable file.

| OS | Command |
| :--- | :--- |
| **Windows** | `mingw32-make` |
| **macOS / Linux** | `make` |

### 2. Run the Application
Once compiled, run the executable directly from the terminal. You must provide the paths to your data files as arguments.

**Windows (PowerShell/CMD):**
```powershell
.\ALNS\main_ALNS.exe ..\tc_v1.csv ..\tc_emp1.csv ..\tc_meta1.csv ..\dist.txt

```

**macOS / Linux (Bash/Zsh):**

```bash
./ALNS/main_ALNS.exe ../tc_v1.csv ../tc_emp1.csv ../tc_meta1.csv ../dist.txt

```

> ** Important:** The `..` in the paths assumes your data files (`.csv`, `.txt`) are located one folder *above* your source code folder. If they are in the same folder, remove the `..\` prefix.

---

## Maintenance

To clean up compiled object files (`.o`) and executables:

| OS | Command |
| --- | --- |
| **Windows** | `mingw32-make clean` |
| **macOS / Linux** | `make clean` |

*Use this if you encounter weird compilation errors or want to trigger a full rebuild.*

---

##  Project Structure

* **`main_ALNS.cpp`**: The entry point that orchestrates the VRP logic.
* **`ALNS.cpp`**: Contains the core logic for the Adaptive Large Neighborhood Search.
* **`CSVReader.cpp`**: Handles parsing of Vehicle and Employee CSV files.
* **`CostFunction.cpp`**: Calculates objective costs based on Metadata weights.
* **`DestroyOperators.cpp`**: Heuristics to remove customers from routes.
* **`RepairOperators.cpp`**: Heuristics to re-insert customers into routes.

---

##  Troubleshooting

**1. "mingw32-make" is not recognized**

* **Fix:** You need to add the path to your MinGW `bin` folder to your Windows **Environment Variables**.

**2. "make: *** No rule to make target..."**

* **Fix:** You are likely trying to run `make run ..\file.csv`. Since we are using manual execution, do not use `make run`. Instead, use the explicit `./vrp_runner.exe` command listed in the "Run" section above.

**3. "The system cannot find the file specified"**

* **Fix:** Check your relative paths. Type `ls ..` (Mac) or `dir ..` (Windows) to verify the CSV files are actually in the parent directory.

