# --- Variables ---
TARGET = route_optimizer
SRC = main.cpp
# If you have a matrix.cpp file, uncomment the line below and update OBJ
# SRC_EXT = matrix.cpp 

CXX = g++
CXXFLAGS = -std=c++11 -O3 -Wall -Wextra

# Object files
OBJ = $(SRC:.cpp=.o)

# --- Rules ---

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

# Compile source files into object files
%.o: %.cpp matrix.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build artifacts and output files
clean:
	rm -f $(OBJ) $(TARGET)
	rm -f vehicle_output.csv employee_output.csv

# Run the program after building
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
