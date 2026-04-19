#include "GraphBuilder.h"

#include "matrix.h"
GraphBuilder::GraphBuilder(const std::vector<Request> &requests, const std::vector<Vehicle> &vehicles) {
    n_u = requests.size();
    n_v = vehicles.size();
    n = n_u + n_v;

    max_global_capacity = 0;
    for (const auto &v : vehicles) {
        if (v.max_capacity > max_global_capacity) {
            max_global_capacity = v.max_capacity;
        }
    }

    buildNodes(requests, vehicles);
}

int GraphBuilder::getPickupNodeId(int reqIndex) { return n_v + 1 + reqIndex; }

int GraphBuilder::getDeliveryNodeId(int reqIndex) { return n + n_v + 1 + reqIndex; }

void GraphBuilder::buildNodes(const std::vector<Request> &requests, const std::vector<Vehicle> &vehicles) {
    nodes.clear();

    auto pickupMatrixIdx = [](const std::string &id) { return matrixIdxOf(id); };
    auto vehicleMatrixIdx = [](const std::string &id) { return matrixIdxOf(id); };
    const int officeIdx = g_officeMatrixIdx;

    // 0. Super Source
    Node superSource;
    superSource.id = 0;
    superSource.type = Node::SUPER_SOURCE;
    superSource.demand = 0;
    superSource.earliest_time = 0;
    superSource.latest_time = 24 * 60;
    superSource.service_duration = 0;
    superSource.request_id = -1;
    superSource.vehicle_id = -1;
    superSource.matrix_idx = officeIdx;
    nodes.push_back(superSource);

    // 1. Dummy Pickups
    for (int k = 0; k < n_v; ++k) {
        Node dummyPick;
        dummyPick.id = 1 + k;
        dummyPick.type = Node::DUMMY_START;
        dummyPick.demand = max_global_capacity - vehicles[k].max_capacity;
        dummyPick.earliest_time = vehicles[k].available_from;
        dummyPick.latest_time = 24 * 60;
        dummyPick.service_duration = 0;
        dummyPick.request_id = -1;
        dummyPick.vehicle_id = k;
        dummyPick.matrix_idx = vehicleMatrixIdx(vehicles[k].original_id);
        nodes.push_back(dummyPick);
    }

    // 2. Real Pickups
    for (int i = 0; i < n_u; ++i) {
        Node pickup;
        pickup.id = n_v + 1 + i;
        pickup.type = Node::PICKUP;
        pickup.demand = 1;
        pickup.earliest_time = requests[i].earliest_pickup;
        pickup.latest_time = 24 * 60;
        pickup.service_duration = 0;
        pickup.request_id = i;
        pickup.vehicle_id = -1;
        pickup.matrix_idx = pickupMatrixIdx(requests[i].original_id);
        nodes.push_back(pickup);
    }

    // 3. Dummy Deliveries
    for (int k = 0; k < n_v; ++k) {
        Node dummyDrop;
        dummyDrop.id = n + 1 + k;
        dummyDrop.type = Node::DUMMY_END;
        dummyDrop.demand = -(max_global_capacity - vehicles[k].max_capacity);
        dummyDrop.earliest_time = 0;
        dummyDrop.latest_time = 24 * 60;
        dummyDrop.service_duration = 0;
        dummyDrop.request_id = -1;
        dummyDrop.vehicle_id = k;
        dummyDrop.matrix_idx = officeIdx;
        nodes.push_back(dummyDrop);
    }

    // 4. Real Deliveries
    for (int i = 0; i < n_u; ++i) {
        Node delivery;
        delivery.id = n + n_v + 1 + i;
        delivery.type = Node::DELIVERY;
        delivery.demand = -1;
        delivery.earliest_time = 0;
        delivery.latest_time = requests[i].latest_drop;
        delivery.service_duration = 0;
        delivery.request_id = i;
        delivery.vehicle_id = -1;
        delivery.matrix_idx = officeIdx;
        nodes.push_back(delivery);
    }

    // 5. Super Sink
    Node superSink;
    superSink.id = 2 * n + 1;
    superSink.type = Node::SUPER_SINK;
    superSink.demand = 0;
    superSink.earliest_time = 0;
    superSink.latest_time = 24 * 60;
    superSink.service_duration = 0;
    superSink.request_id = -1;
    superSink.vehicle_id = -1;
    superSink.matrix_idx = officeIdx;
    nodes.push_back(superSink);
}