
#include "CostFunction.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Distance.h"

static double priorityPenalty(int p) { return 0.0 * (6 - p) * (6 - p); }

SplitResult evaluateRouteDP(const std::vector<int>& seq, const Vehicle& v, const std::vector<Employee>& emp,
                            const Metadata& meta) {
    int n = seq.size();
    if (n == 0) return {{0.0, 0.0, 0.0}, v.startTime, {}};
    if (v.speed <= 0.0) return {{1e100, 1e100, 1e100}, v.startTime, {}};

    struct DPState {
        double cost;
        double penalty;
        double opCost;
        double endTime;
        double endX;
        double endY;
        int prev;
    };

    std::vector<DPState> dp(n + 1);
    for (int i = 0; i <= n; i++) dp[i].cost = 1e100;

    dp[0] = {0.0, 0.0, 0.0, v.startTime, v.x, v.y, -1};

    for (int i = 1; i <= n; i++) {
        for (int j = 0; j < i; j++) {
            if (dp[j].cost >= 1e99) continue;

            int batchSize = i - j;
            if (batchSize > v.seatCap) continue;

            bool validShare = true;
            for (int k = j; k < i; k++) {
                if (emp[seq[k]].sharePref < batchSize) {
                    validShare = false;
                    break;
                }
            }
            if (!validShare) continue;

            double currT = dp[j].endTime;
            double cx = dp[j].endX;
            double cy = dp[j].endY;

            double batchOpCost = 0.0;
            double batchPenalty = 0.0;
            double travelDist = 0.0;
            double duration = 0.0;

            std::vector<double> pickupTimes(batchSize);

            for (int k = j; k < i; k++) {
                const auto& e = emp[seq[k]];

                if (e.vehiclePref == "premium" && !v.premium) {
                    batchPenalty += 100000.0;
                }

                double d = distKm(cx, cy, e.x, e.y);
                travelDist += d;
                currT += (d / v.speed) * 60.0;

                double startService = std::max(currT, e.ready);
                currT = startService;
                pickupTimes[k - j] = currT;

                cx = e.x;
                cy = e.y;
            }

            const auto& last = emp[seq[i - 1]];
            double dOff = distKm(cx, cy, last.destX, last.destY);
            travelDist += dOff;
            currT += (dOff / v.speed) * 60.0;

            cx = last.destX;
            cy = last.destY;

            for (int k = j; k < i; k++) {
                const auto& e = emp[seq[k]];
                duration += (currT - pickupTimes[k - j]);

                double due = e.due + getMaxLateness(e.priority, meta);
                if (currT > due) {
                    double lateMins = currT - due;
                    batchPenalty += 100000.0 * lateMins;
                }
            }

            double travelMoneyCost = travelDist * v.costPerKm;
            batchOpCost = (travelMoneyCost * meta.objectiveCostWeight) + (duration * meta.objectiveTimeWeight);

            double totalBatchCost = batchOpCost + batchPenalty;

            double newTime = currT;
            double newCost = dp[j].cost + totalBatchCost;
            double newPenalty = dp[j].penalty + batchPenalty;
            double newOpCost = dp[j].opCost + batchOpCost;

            if (newCost < dp[i].cost) {
                dp[i] = {newCost, newPenalty, newOpCost, newTime, cx, cy, j};
            }
        }
    }

    if (dp[n].cost >= 1e99) {
        return {{1e100, 1e100, 1e100}, v.startTime, {}};
    }

    if (dp[n].endTime > v.endTime) {
        dp[n].penalty += 10000.0 * (dp[n].endTime - v.endTime);
        dp[n].cost += 10000.0 * (dp[n].endTime - v.endTime);
    }

    std::vector<int> splits;
    int curr = n;
    while (curr > 0) {
        splits.push_back(dp[curr].prev);
        curr = dp[curr].prev;
    }
    std::reverse(splits.begin(), splits.end());

    return {{dp[n].opCost, dp[n].penalty, dp[n].cost}, dp[n].endTime, splits};
}

CostComponents getRouteCostComponents(const Route& r, const Vehicle& v, const std::vector<Employee>& emp,
                                      const Metadata& meta) {
    CostComponents cc = {0.0, 0.0, 0.0};
    if (v.speed <= 0.0) {
        cc = {1e100, 1e100, 1e100};
        return cc;
    }
    if (r.seq.empty()) return cc;

    return evaluateRouteDP(r.seq, v, emp, meta).cost;
}

double routeCost(const Route& r, const Vehicle& v, const std::vector<Employee>& emp, const Metadata& meta) {
    if (r.seq.empty()) return 0.0;
    if (!r.isDirty) return r.cachedCost;

    CostComponents cc = getRouteCostComponents(r, v, emp, meta);

    r.cachedCost = cc.totalCost;
    r.isDirty = false;
    return r.cachedCost;
}
