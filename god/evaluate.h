#pragma once
#include "utils.h"
#include "model.h"
#include <utility>

// Calculates vehicle waiting cost based on stepwise tariff.
double get_waiting_cost(double duration, char tariff_type, const DARPInstance &instance);

// Computes A_i, B_i, W_i, L_i, costs, and violations for a single route.
void calculate_route_stats(Route &route, DARPInstance &instance);

// Aggregates costs from all routes and applies penalties
double evaluate_solution(Solution &solution, DARPInstance &instance, double psi = 1.0);

// Incrementally updates solution costs — only recalculates routes with stats_valid == false
double update_solution_costs(Solution &solution, DARPInstance &instance, double psi = 1.0);

// Helper to calculate specific route cost for delta evaluation.
double calculate_route_cost_delta(Route &route, DARPInstance &instance, double alpha, double beta, double gamma);

// Tries to insert request (Pickup P, Delivery D) into route.
// Finds position minimizing cost increase.
std::pair<double, std::vector<int>> insert_request_best_position(Route &route, const Request &request, DARPInstance &instance, double alpha, double beta, double gamma);

// Validates that all requests are accounted for in routes + unassigned list
void validate_solution_integrity(const Solution &solution, const DARPInstance &instance);
