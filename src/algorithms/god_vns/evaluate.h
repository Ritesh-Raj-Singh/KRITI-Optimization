#pragma once
#include <utility>

#include "model.h"
#include "utils.h"

double get_waiting_cost(double duration, char tariff_type, const DARPInstance &instance);

void calculate_route_stats(Route &route, DARPInstance &instance);

double evaluate_solution(Solution &solution, DARPInstance &instance, double psi = 1.0);

double update_solution_costs(Solution &solution, DARPInstance &instance, double psi = 1.0);

double calculate_route_cost_delta(Route &route, DARPInstance &instance, double alpha, double beta, double gamma);

std::pair<double, std::vector<int>> insert_request_best_position(Route &route, const Request &request,
                                                                 DARPInstance &instance, double alpha, double beta,
                                                                 double gamma);

void validate_solution_integrity(const Solution &solution, const DARPInstance &instance);
