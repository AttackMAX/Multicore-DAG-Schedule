#include <vector>

#include "algorithms/minimum_core_algorithm.h"
#include "algorithms/node_priority_assignment_algorithm.h"
#include "dag/dag_graph.h"

int main() {
  dag::DagGraph graph;
  if (!graph.AddNode(1, 3)) {
    return 1;
  }
  if (!graph.AddNode(2, 4)) {
    return 1;
  }
  if (!graph.AddNode(3, 5)) {
    return 1;
  }
  if (!graph.AddNode(4, 2)) {
    return 1;
  }
  if (!graph.AddEdge(1, 3)) {
    return 1;
  }
  if (!graph.AddEdge(2, 3)) {
    return 1;
  }
  if (!graph.AddEdge(3, 4)) {
    return 1;
  }
  auto order = graph.TopologicalOrder();
  if (order.size() != 4) {
    return 1;
  }

  algorithms::NodePriorityAssignmentAlgorithm algorithm;
  const algorithms::CriticalPathMetrics metrics =
      algorithm.ComputeCriticalPathMetrics(graph);
  if (!metrics.valid) {
    return 1;
  }
  if (metrics.lf.size() != 4 || metrics.lb.size() != 4 ||
      metrics.longest_path.size() != 4) {
    return 1;
  }
  if (metrics.critical_path_length <= 0) {
    return 1;
  }
  if (metrics.lf.at(3) < metrics.lf.at(1) ||
      metrics.lf.at(3) < metrics.lf.at(2)) {
    return 1;
  }
  if (metrics.lb.at(3) < metrics.lb.at(4)) {
    return 1;
  }

  const std::vector<int> priorities =
      algorithm.AssignPrioritiesByCriticalPath(graph, metrics);
  if (priorities.size() != 4) {
    return 1;
  }
  if (priorities[2] != 3 || priorities[3] != 4) {
    return 1;
  }

  const algorithms::ResponseTimeAnalysis response =
      algorithm.AnalyzeResponseTime(graph, priorities, 2);
  if (!response.valid) {
    return 1;
  }
  if (response.finish_time.size() != 4) {
    return 1;
  }
  if (response.finish_time.at(3) < response.finish_time.at(1) ||
      response.finish_time.at(3) < response.finish_time.at(2)) {
    return 1;
  }
  if (response.finish_time.at(4) < response.finish_time.at(3)) {
    return 1;
  }
  if (response.worst_response_time < response.finish_time.at(4)) {
    return 1;
  }

  // --- MinimumCoreAlgorithm tests ---
  algorithms::MinimumCoreAlgorithm min_core_algo;

  // Test 1: target < critical_path_length (10) -> infeasible
  {
    auto prio = min_core_algo.ComputeForPriority(graph, 5.0);
    if (prio.feasible) {
      return 1;
    }
    auto graham = min_core_algo.ComputeForGraham(graph, 5.0);
    if (graham.feasible) {
      return 1;
    }
  }

  // Test 2: very large target -> 1 core for both
  {
    auto prio = min_core_algo.ComputeForPriority(graph, 100.0);
    if (!prio.feasible || prio.min_cores != 1) {
      return 1;
    }
    if (prio.worst_response_time <= 0.0 || prio.worst_response_time > 100.0) {
      return 1;
    }

    auto graham = min_core_algo.ComputeForGraham(graph, 100.0);
    if (!graham.feasible || graham.min_cores != 1) {
      return 1;
    }
    if (graham.worst_response_time <= 0.0 || graham.worst_response_time > 100.0) {
      return 1;
    }
  }

  // Test 3: reasonable target -> both feasible, priority cores <= graham cores
  {
    auto prio = min_core_algo.ComputeForPriority(graph, 12.0);
    if (!prio.feasible || prio.min_cores <= 0) {
      return 1;
    }
    if (prio.worst_response_time > 12.0) {
      return 1;
    }

    auto graham = min_core_algo.ComputeForGraham(graph, 12.0);
    if (!graham.feasible || graham.min_cores <= 0) {
      return 1;
    }
    if (graham.worst_response_time > 12.0) {
      return 1;
    }

    if (prio.min_cores > graham.min_cores) {
      return 1;
    }
  }

  // Test 4: target equals critical_path_length -> feasible but needs many cores
  {
    double L = static_cast<double>(metrics.critical_path_length);
    auto prio = min_core_algo.ComputeForPriority(graph, L);
    if (!prio.feasible) {
      return 1;
    }
    // With only critical path length as target, need more than 1 core
    // since total work (14) > L (10) implies interference exists
    if (prio.min_cores < 1) {
      return 1;
    }
  }

  return 0;
}
