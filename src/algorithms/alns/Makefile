# Detect OS
ifeq ($(OS),Windows_NT)
    # Windows settings
    RM = del /Q /F
    TARGET = main_ALNS
    FixPath = $(subst /,\,$1)
    EXEC = $(TARGET)
else
    # Mac/Linux settings
    RM = rm -f
    TARGET = main_ALNS
    FixPath = $1
    EXEC = ./$(TARGET)
endif

# Variables
CXX = g++
CXXFLAGS = -std=c++17 -O3
SRCS = main_ALNS.cpp ALNS.cpp CSVReader.cpp CostFunction.cpp DestroyOperators.cpp RepairOperators.cpp globals.cpp
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Cross-platform clean
clean:
	$(RM) *.o $(TARGET) output_vehicle.csv output_employees.csv

# # Cross-platform run
# run: all
# 	$(EXEC)