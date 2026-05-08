#include "algorithms/minimum_core_algorithm.h"

#include <functional>

#include "algorithms/graham_response_time_analysis.h"
#include "algorithms/node_priority_assignment_algorithm.h"

namespace algorithms {

namespace {

int FindMinCores(int max_cores, std::function<bool(int)> is_feasible) {
  int low = 0;
  int high = 0;

  int m = 1;
  while (m <= max_cores) {
    if (is_feasible(m)) {
      high = m;
      break;
    }
    low = m;
    m *= 2;
  }

  if (high == 0) {
    if (is_feasible(max_cores)) {
      high = max_cores;
      low = m / 2;
    } else {
      return -1;
    }
  }

  while (low + 1 < high) {
    int mid = (low + high) / 2;
    if (is_feasible(mid)) {
      high = mid;
    } else {
      low = mid;
    }
  }

  return high;
}

}  // namespace

MinimumCoreResult MinimumCoreAlgorithm::ComputeForPriority(
    const dag::DagGraph& graph, double target_response_time,
    int max_cores) const {
  MinimumCoreResult result;
  if (max_cores <= 0) {
    return result;
  }

  NodePriorityAssignmentAlgorithm algo;

  auto metrics = algo.ComputeCriticalPathMetrics(graph);
  if (!metrics.valid) {
    return result;
  }

  if (static_cast<double>(metrics.critical_path_length) > target_response_time) {
    return result;
  }

  auto priorities = algo.AssignPrioritiesByCriticalPath(graph, metrics);
  if (priorities.empty()) {
    return result;
  }

  auto is_feasible = [&](int m) {
    auto response = algo.AnalyzeResponseTime(graph, priorities, m);
    return response.valid &&
           response.worst_response_time <= target_response_time;
  };

  int min_cores = FindMinCores(max_cores, is_feasible);
  if (min_cores < 0) {
    return result;
  }

  result.min_cores = min_cores;
  result.feasible = true;
  auto final_response = algo.AnalyzeResponseTime(graph, priorities, min_cores);
  result.worst_response_time = final_response.worst_response_time;
  return result;
}

MinimumCoreResult MinimumCoreAlgorithm::ComputeForGraham(
    const dag::DagGraph& graph, double target_response_time,
    int max_cores) const {
  MinimumCoreResult result;
  if (max_cores <= 0) {
    return result;
  }

  GrahamResponseTimeAlgorithm graham_algo;

  auto probe = graham_algo.Analyze(graph, 1);
  if (!probe.valid) {
    return result;
  }

  if (static_cast<double>(probe.critical_path_length) > target_response_time) {
    return result;
  }

  auto is_feasible = [&](int m) {
    auto response = graham_algo.Analyze(graph, m);
    return response.valid &&
           response.worst_response_time <= target_response_time;
  };

  int min_cores = FindMinCores(max_cores, is_feasible);
  if (min_cores < 0) {
    return result;
  }

  result.min_cores = min_cores;
  result.feasible = true;
  auto final_response = graham_algo.Analyze(graph, min_cores);
  result.worst_response_time = final_response.worst_response_time;
  return result;
}

}  // namespace algorithms
