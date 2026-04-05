#include "dag/dag_graph.h"

int main() {
  dag::DagGraph graph;
  if (!graph.AddNode(1, 10)) {
    return 1;
  }
  if (!graph.AddNode(2, 20)) {
    return 1;
  }
  if (!graph.AddEdge(1, 2)) {
    return 1;
  }
  auto order = graph.TopologicalOrder();
  if (order.size() != 2) {
    return 1;
  }
  if (order[0] != 1 || order[1] != 2) {
    return 1;
  }
  return 0;
}
