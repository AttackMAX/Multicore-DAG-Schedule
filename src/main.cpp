#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "algorithms/dag_generator.h"
#include "algorithms/graham_response_time_analysis.h"
#include "algorithms/node_priority_assignment_algorithm.h"
#include "dag/dag_graph.h"

int main(int argc, char* argv[]) {
  dag::DagGraph graph;

  std::string dag_name;
  for (int i = 1; i < argc - 1; ++i) {
    if (std::string(argv[i]) == "--dag") {
      dag_name = argv[i + 1];
      break;
    }
  }

  if (!dag_name.empty()) {
    auto generated = algorithms::DAGGenerator::Resolve(dag_name);
    graph = generated.graph;
    if (graph.NodeCount() == 0) {
      std::cerr << "Failed to load DAG: " << dag_name << "\n";
      return 1;
    }
  } else {
    graph.AddNode(1, 5);
    graph.AddNode(2, 3);
    graph.AddNode(3, 8);
    graph.AddNode(4, 2);
    graph.AddEdge(1, 3);
    graph.AddEdge(2, 3);
    graph.AddEdge(3, 4);
  }

  const int core_count = 2;

  algorithms::NodePriorityAssignmentAlgorithm priority_algo;
  const algorithms::CriticalPathMetrics metrics =
      priority_algo.ComputeCriticalPathMetrics(graph);
  if (!metrics.valid) {
    std::cout << "Critical path metrics failed\n";
    return 1;
  }

  const std::vector<int> priorities =
      priority_algo.AssignPrioritiesByCriticalPath(graph, metrics);
  if (priorities.empty()) {
    std::cout << "Priority assignment failed\n";
    return 1;
  }

  const algorithms::ResponseTimeAnalysis priority_response =
      priority_algo.AnalyzeResponseTime(graph, priorities, core_count);
  if (!priority_response.valid) {
    std::cout << "Priority response time analysis failed\n";
    return 1;
  }

  algorithms::GrahamResponseTimeAlgorithm graham_algo;
  const algorithms::GrahamResponseTimeAnalysis graham_response =
      graham_algo.Analyze(graph, core_count);
  if (!graham_response.valid) {
    std::cout << "Graham response time analysis failed\n";
    return 1;
  }

  std::cout << "=== DAG Info ===\n";
  std::cout << "Nodes: " << graph.NodeCount() << "\n";
  std::cout << "Critical path length (L): " << metrics.critical_path_length
            << "\n";
  std::cout << "Total work (W): " << graham_response.total_work << "\n";
  std::cout << "Core count (m): " << core_count << "\n\n";

  std::cout << "=== Priority-Based Scheduling ===\n";
  std::cout << "Priority order:";
  for (int id : priorities) std::cout << " " << id;
  std::cout << "\n";
  std::cout << "Worst response time: " << priority_response.worst_response_time
            << "\n";
  for (int id : graph.TopologicalOrder()) {
    std::cout << "  f_prio(" << id << ") = "
              << priority_response.finish_time.at(id) << "\n";
  }

  std::cout << "\n=== Graham Bound (Arbitrary Scheduling) ===\n";
  std::cout << "Makespan bound L+(W-L)/m: "
            << graham_response.graham_makespan_bound << "\n";
  std::cout << "Worst response time: " << graham_response.worst_response_time
            << "\n";
  for (int id : graph.TopologicalOrder()) {
    std::cout << "  f_grm(" << id << ") = "
              << graham_response.finish_time.at(id) << "\n";
  }

  std::cout << "\n=== Comparison ===\n";
  if (priority_response.worst_response_time <=
      graham_response.worst_response_time) {
    std::cout << "Priority-based worst response time ("
              << priority_response.worst_response_time
              << ") <= Graham worst response time ("
              << graham_response.worst_response_time << ")\n";
    std::cout << "Tightening ratio: "
              << (priority_response.worst_response_time /
                  graham_response.worst_response_time)
              << "\n";
  } else {
    std::cout << "ERROR: priority bound exceeds Graham bound\n";
    return 1;
  }

  return 0;
}
