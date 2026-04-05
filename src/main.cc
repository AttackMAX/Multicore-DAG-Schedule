#include <iostream>
#include <memory>
#include <vector>

#include "algorithms/critical_path_scheduler.h"
#include "algorithms/list_baseline_scheduler.h"
#include "dag/dag_graph.h"
#include "scheduler/task_scheduler.h"

int main() {
  dag::DagGraph graph;
  graph.AddNode(1, 5);
  graph.AddNode(2, 3);
  graph.AddNode(3, 8);
  graph.AddNode(4, 2);

  graph.AddEdge(1, 3);
  graph.AddEdge(2, 3);
  graph.AddEdge(3, 4);

  std::vector<std::unique_ptr<scheduler::TaskScheduler>> schedulers;
  schedulers.push_back(std::make_unique<algorithms::CriticalPathScheduler>());
  schedulers.push_back(std::make_unique<algorithms::ListBaselineScheduler>());

  for (const auto& scheduler : schedulers) {
    auto result = scheduler->Schedule(graph, 4);
    std::cout << scheduler->Name() << ": feasible=" << std::boolalpha
              << result.feasible << ", makespan=" << result.makespan << "\n";
  }

  return 0;
}
