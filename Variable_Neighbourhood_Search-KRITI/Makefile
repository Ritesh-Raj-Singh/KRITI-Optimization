# --- Variables ---
CXX      = g++
CXXFLAGS = -std=c++11 -Wall -O2
TARGET   = main_vns
SRC      = main_vns.cpp

# --- Targets ---

# Default target: Compile the program
all: $(TARGET)

# Rule to link the executable
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

# Rule to run the program
# Usage: make run
# NOTE: Ensure the CSV files exist in the folder before running this
run: $(TARGET)
	./$(TARGET) vehicles.csv employees.csv metadata.csv distance_matrix.csv

# Clean up build files and output CSVs
clean:
	del $(TARGET).exe vehicle_output.csv employee_output.csv 2>NUL || rm -f $(TARGET) $(TARGET).exe vehicle_output.csv employee_output.csv
