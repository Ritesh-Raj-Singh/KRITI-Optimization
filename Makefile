# Compiler settings
CXX      := g++
# Flags: C++17, Threading, Optimization, Asio Include path + Standalone Flag
CXXFLAGS := -std=c++17 -pthread -Wall -O3 -Iasio-1.28.0/include -DASIO_STANDALONE

# ---------------- CONFIGURATION ----------------
# 1. List your subdirectory names here (space separated)
# This ensures 'make' goes into these folders and compiles the solvers first
SUBDIRS  := ALNS Branch-And-Cut Clustering-Routing-DP-Solver Heterogeneous_DARP Variable_Neighbourhood_Search god memetic_algorithm

# 2. Name your final server executable
TARGET   := server_app

# 3. Your main file
SRC      := main.cpp
# -----------------------------------------------

.PHONY: all subdirs $(SUBDIRS) clean

# Default Rule: Build subdirs FIRST, then the main server
all: subdirs $(TARGET)

# Rule to handle subdirectories
subdirs: $(SUBDIRS)

$(SUBDIRS):
	@echo "====== Building inside $@ ======"
	$(MAKE) -C $@

# Rule to build the server
$(TARGET): $(SRC)
	@echo "====== Building Main Server ======"
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# Clean Rule
clean:
	@echo "[Root] Removing executable and temp files..."
	rm -f $(TARGET)
	# Remove temporary files generated during runtime
	rm -f vehicles.csv employees.csv metadata.csv baseline.csv matrix.txt osrm_raw.json run_log.txt
	
	@for dir in $(SUBDIRS); do \
		echo "Cleaning $$dir..."; \
		$(MAKE) -C $$dir clean; \
	done