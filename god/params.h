#pragma once

namespace Params
{
    // ── Violation Penalty Coefficients ────────────────────────────────────────
    // These multiply constraint violations in f3 = alpha*viol_ride + beta*viol_tw + gamma*viol_cap.
    // Higher values make the solver treat violations as more costly → fewer feasibility violations
    // in output, but may cause the VNS to accept worse monetary cost solutions to stay feasible.
    inline constexpr double ALPHA = 1.0;    // Ride-time violation: low keeps max-ride-time soft
    inline constexpr double BETA = 5.0;     // Time-window violation: soft-medium enforcement
                                            // (viol_tw units = min×priority, typically 50–500 total;
                                            //  100 units × 5 = 500 ≈ 29% of f1=1750 → explorable but penalized)
    inline constexpr double GAMMA = 1000.0; // Capacity violation: semi-hard enforcement
                                            // (viol_cap units = persons over cap, typically 2–10 total;
                                            //  3 units × 150 = 450 ≈ 26% of f1=1750 → costly but explorable
                                            //  to allow VNS to escape local optima through infeasible states)

    // ── Objective Weighting ───────────────────────────────────────────────────
    // F2_WEIGHT scales the employee-quality score (f2) relative to monetary cost (f1).
    // Increase → optimizer prioritizes passenger punctuality; decrease → prioritizes cost.
    inline constexpr double F2_WEIGHT = 0.25;

    // LOCAL_SEARCH_PSI is the f2 weight used specifically during update_solution_costs
    // inside local search. Matches F2_WEIGHT by default; set lower to de-emphasize
    // quality during the local search phase.
    inline constexpr double LOCAL_SEARCH_PSI = 0.25;

    // ── Penalty for Unserved Requests ─────────────────────────────────────────
    // Added to f3 for each request that cannot be inserted into any route.
    // Very high → VNS strongly avoids leaving requests unassigned.
    // Lower → solver may trade unassigned requests for better route costs.
    inline constexpr double UNASSIGNED_PENALTY = 10000.0;

    // ── Local Search Improvement Threshold ────────────────────────────────────
    // A repositioning is accepted only if it improves route cost by more than this.
    // Larger → local search is coarser/faster; smaller → more precise but slower.
    inline constexpr double IMPROVEMENT_EPS = 1.0;

    // ── VNS Search Budget (CLI defaults) ──────────────────────────────────────
    // Override at runtime: ./solver <input> <output> ... <max_iter> <h>
    // DEFAULT_MAX_ITERATIONS: more iterations → better solution quality, longer runtime.
    inline constexpr long long DEFAULT_MAX_ITERATIONS = 250000;

    // DEFAULT_H: neighborhood depth — requests moved/swapped per perturbation step.
    // Higher → more disruptive moves, escapes local optima more easily,
    // but individual moves are less targeted.
    inline constexpr int DEFAULT_H = 5;
}
