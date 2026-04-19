#include <map>
#include <vector>

#include "FeasibilityChecker.h"
#include "GraphBuilder.h"
#include "globals.h"
#include "structures.h"
#include "utils.h"

class Solver {
   public:
    struct Solution {
        std::map<int, std::vector<int>> routes;
        std::vector<int> unserved_requests;
        double total_cost;
        int vehicles_used;
    };

    Solver(const std::vector<Request> &r, const std::vector<Vehicle> &v, GraphBuilder &gb);
    long long getRoutePenalty(const std::vector<int> &route, int veh_idx);

    double dist_cost = 1.0;
    double time_cost = 0.0;

    double param_t_max_mult = 100.0;
    int param_t_red = 2000;
    int param_n_imp = 1000;
    int max_iterations = 50000;

    void buildInitialSolution(Solution &sol);
    void buildRegretSolution(Solution &sol);
    bool insertRequestBest(Solution &sol, int req_idx);
    bool insertRequestBest(Solution &sol, int req_idx, int forbidden_veh_id);

    void buildGraphMatchingInitialSolution(Solution &sol);

    Solution solveDeterministicAnnealing();

   private:
    std::vector<Request> requests;
    std::vector<Vehicle> vehicles;
    GraphBuilder &graph;
    FeasibilityChecker checker;

    double calculateTotalCost(const Solution &sol);
    double calculateRouteCost(const std::vector<int> &route, const Vehicle &v);

    double calculateAverageEdgeCost();

    bool operatorRelocate(Solution &current_sol, double threshold);
    bool operatorEliminate(Solution &sol);
    bool operator2Opt(Solution &sol, double threshold);
    bool operatorExchange(Solution &current_sol, double threshold);
    bool operatorInsertUnserved(Solution &sol);
};