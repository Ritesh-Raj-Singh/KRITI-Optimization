CXX := g++
CXXFLAGS := -std=c++17 -Wall -O3 -pthread -Iinclude -Iinclude/third_party -DASIO_STANDALONE

BUILD_DIR := build
BIN_DIR := build/bin
SRC_DIR := src

# Determine all algorithm directories dynamically
ALGO_DIRS := $(wildcard $(SRC_DIR)/algorithms/*)
# For each algorithm directory, find all .cpp files
ALGO_SRCS := $(foreach dir, $(ALGO_DIRS), $(wildcard $(dir)/*.cpp))

# Extract the main files for algorithms - assuming they all have a main_*.cpp or evaluate/vns/god.cpp 
# For simplicity, we will specify the targets we want to build
TARGET_ALNS := $(BIN_DIR)/main_alns
TARGET_BAC := $(BIN_DIR)/main_bac
TARGET_CRDP := $(BIN_DIR)/main_crdp
TARGET_HETERO := $(BIN_DIR)/main_hetero
TARGET_VNS := $(BIN_DIR)/main_vns
TARGET_GOD := $(BIN_DIR)/main_god
TARGET_MEMETIC := $(BIN_DIR)/main_memetic
TARGET_SERVER := $(BIN_DIR)/server_app

SERVER_SRC := $(SRC_DIR)/server/main.cpp

# List of all final executables
TARGETS := $(TARGET_SERVER) $(TARGET_ALNS) $(TARGET_BAC) $(TARGET_CRDP) $(TARGET_HETERO) $(TARGET_VNS) $(TARGET_GOD) $(TARGET_MEMETIC)

.PHONY: all clean directories

all: directories $(TARGETS)

directories:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(BUILD_DIR)/obj

# Server compilation
$(TARGET_SERVER): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

# ALNS compilation
$(TARGET_ALNS): $(wildcard $(SRC_DIR)/algorithms/alns/*.cpp)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Branch And Cut compilation
$(TARGET_BAC): $(wildcard $(SRC_DIR)/algorithms/branch_and_cut/*.cpp)
	$(CXX) $(CXXFLAGS) -o $@ $^

# CRDP compilation
$(TARGET_CRDP): $(wildcard $(SRC_DIR)/algorithms/crdp/*.cpp)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Heterogeneous DARP compilation
$(TARGET_HETERO): $(wildcard $(SRC_DIR)/algorithms/heterogeneous_darp/*.cpp)
	$(CXX) $(CXXFLAGS) -o $@ $^

# VNS compilation
$(TARGET_VNS): $(wildcard $(SRC_DIR)/algorithms/vns/*.cpp)
	$(CXX) $(CXXFLAGS) -o $@ $^

# GOD compilation
$(TARGET_GOD): $(wildcard $(SRC_DIR)/algorithms/god_vns/*.cpp)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Memetic compilation
$(TARGET_MEMETIC): $(wildcard $(SRC_DIR)/algorithms/memetic/*.cpp)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR)
	rm -f *.csv *.txt
