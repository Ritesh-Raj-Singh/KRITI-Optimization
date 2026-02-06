# --- Variables ---
# Name of your source file (without extension)
SRC = main
# Name of the output executable
TARGET = route_optimizer

# Compiler
CXX = g++
# Compiler flags: 
# -std=c++11: Required for your map/vector usage
# -O3: High-level optimization for your 100,000 iterations
# -Wall: Show all warnings
CXXFLAGS = -std=c++11 -O3 -Wall

# --- Rules ---

# Default target
all: $(TARGET)

# Link the object files to create the executable
$(TARGET): $(SRC).o
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC).o

# Compile the source file into an object file
$(SRC).o: $(SRC).cpp
	$(CXX) $(CXXFLAGS) -c $(SRC).cpp

# Clean up build files
clean:
	rm -f $(SRC).o $(TARGET)
	rm -f vehicle_output.csv employee_output.csv

# Run the program
run: $(TARGET)
	./$(TARGET)
