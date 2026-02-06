# Detect OS
ifeq ($(OS),Windows_NT)
    # Windows settings
    RM = del /Q /F
    TARGET = main_BAC
    FixPath = $(subst /,\,$1)
    EXEC = $(TARGET)
else
    # Mac/Linux settings
    RM = rm -f
    TARGET = main_BAC
    FixPath = $1
    EXEC = ./$(TARGET)
endif

# Variables
CXX = g++
CXXFLAGS = -std=c++17 -O3
SRCS = main_BAC.cpp matrix.cpp utils.cpp FeasibilityChecker.cpp GraphBuilder.cpp Solver.cpp 
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