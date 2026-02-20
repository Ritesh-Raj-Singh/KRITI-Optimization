# ── Stage 1: Builder ──────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install build tools and runtime dependencies needed at compile time
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    g++ \
    make \
    cmake \
    wget \
    curl \
    ca-certificates \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Download Asio (standalone, header-only) — same version as in Makefile
RUN wget -q https://sourceforge.net/projects/asio/files/asio/1.28.0%20%28Stable%29/asio-1.28.0.tar.gz/download \
         -O asio.tar.gz \
    && tar -xzf asio.tar.gz \
    && rm asio.tar.gz

# Copy the entire project (all sources, subdirs, headers)
COPY . .

# Build everything: subdirectory solvers first, then the main server
RUN make all

# ── Stage 2: Runtime ──────────────────────────────────────────────────────────
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
    libstdc++6 \
    libboost-system1.74.0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiled server binary
COPY --from=builder /app/server_app .

# Copy solver binaries (each subdirectory executable)
COPY --from=builder /app/ALNS/main_ALNS                                   ./ALNS/main_ALNS
COPY --from=builder /app/Branch-And-Cut/main_BAC                          ./Branch-And-Cut/main_BAC
COPY --from=builder /app/Clustering-Routing-DP-Solver/main_crds           ./Clustering-Routing-DP-Solver/main_crds
COPY --from=builder /app/Heterogeneous_DARP/hetero                        ./Heterogeneous_DARP/hetero
COPY --from=builder /app/Variable_Neighbourhood_Search/main_vns           ./Variable_Neighbourhood_Search/main_vns
COPY --from=builder /app/god/god                                           ./god/god
COPY --from=builder /app/memetic_algorithm/main_Memetic                   ./memetic_algorithm/main_Memetic

# Render expects the app to listen on $PORT; default to 5555 if not set.
# The server is hardcoded to 5555 — set PORT=5555 in Render's environment variables,
# or patch the source to read process.env PORT if you need dynamic binding.
EXPOSE 5555

CMD ["./server_app"]
