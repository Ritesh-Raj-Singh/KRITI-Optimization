# Compiler Settings
CXX      := g++
CXXFLAGS := -std=c++17 -O3 -Wall -pthread

# Target Executable Name
TARGET   := main_ALNS

# Automatically find all .cpp files in this folder
SRCS     := $(wildcard *.cpp)
# Generate corresponding .o file names
OBJS     := $(SRCS:.cpp=.o)

# Default Rule: Build the executable
all: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS)
	@echo "[ALNS] Linking $(TARGET)..."
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Compile .cpp files into .o files
%.o: %.cpp
	@echo "[ALNS] Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	@echo "[ALNS] Cleaning..."
	rm -f $(OBJS) $(TARGET) output_vehicle.csv output_employees.csv run_log.txt

.PHONY: all clean