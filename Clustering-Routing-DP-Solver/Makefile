# ---------------- CONFIGURATION ----------------
# Compiler Settings
CXX      := g++
# Flags: C++17 is required for your code. -O3 for optimization.
CXXFLAGS := -std=c++17 -O3 -Wall -pthread

# Target Executable Name
# IMPORTANT: If this is the "Branch-And-Cut" folder, keep it as main_BAC
# If this is the "ALNS" folder, change it to main_ALNS
TARGET   := main_crds

# ---------------- AUTOMATION ----------------
# 1. Automatically find all .cpp files in this folder
# (e.g., main.cpp, Solver.cpp, GraphBuilder.cpp, utils.cpp, matrix.cpp)
SRCS     := $(wildcard *.cpp)

# 2. Generate corresponding .o file names
OBJS     := $(SRCS:.cpp=.o)

# ---------------- RULES ----------------

# Default Rule: Build the executable
all: $(TARGET)

# Link Step: Combine all .o files into the executable
$(TARGET): $(OBJS)
	@echo "[$(TARGET)] Linking..."
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Compile Step: Turn .cpp into .o
%.o: %.cpp
	@echo "[$(TARGET)] Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files and generated CSVs
clean:
	@echo "[$(TARGET)] Cleaning..."
	rm -f $(OBJS) $(TARGET) output_vehicle.csv output_employees.csv run_log.txt

.PHONY: all clean