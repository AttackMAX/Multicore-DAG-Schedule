#include <iostream>
#include <vector>

#include "algorithms/node_priority_assignment_algorithm.h"
#include "dag/dag_graph.h"

int main() {
  dag::DagGraph graph;
  graph.AddNode(1, 5);
  graph.AddNode(2, 3);
  graph.AddNode(3, 8);
  graph.AddNode(4, 2);

  graph.AddEdge(1, 3);
  graph.AddEdge(2, 3);
  graph.AddEdge(3, 4);

  algorithms::NodePriorityAssignmentAlgorithm algorithm;
  const algorithms::CriticalPathMetrics metrics =
      algorithm.ComputeCriticalPathMetrics(graph);
  if (!metrics.valid) {
    std::cout << "Critical path metrics failed\n";
    return 1;
  }

  std::cout << "Critical path length: " << metrics.critical_path_length << "\n";
  for (int node_id : graph.TopologicalOrder()) {
    std::cout << "Node " << node_id << " lf=" << metrics.lf.at(node_id)
              << " lb=" << metrics.lb.at(node_id)
              << " l=" << metrics.longest_path.at(node_id) << "\n";
  }

  const std::vector<int> priorities =
      algorithm.AssignPrioritiesByCriticalPath(graph, metrics);
  if (priorities.empty()) {
    std::cout << "Priority assignment failed\n";
    return 1;
  }

  std::cout << "Priority order:";
  for (int node_id : priorities) {
    std::cout << " " << node_id;
  }
  std::cout << "\n";

  const algorithms::ResponseTimeAnalysis response =
      algorithm.AnalyzeResponseTime(graph, priorities, 2);
  if (!response.valid) {
    std::cout << "Response time analysis failed\n";
    return 1;
  }
  std::cout << "Worst response time: " << response.worst_response_time << "\n";
  for (int node_id : graph.TopologicalOrder()) {
    std::cout << "f(" << node_id << ")=" << response.finish_time.at(node_id)
              << "\n";
  }

  return 0;
}
