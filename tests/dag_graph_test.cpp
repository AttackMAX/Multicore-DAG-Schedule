#include <vector>

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
  if (metrics.lf.at(3) < metrics.lf.at(1) || metrics.lf.at(3) < metrics.lf.at(2)) {
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
  return 0;
}
