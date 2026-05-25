#ifndef DAG_DAG_GRAPH_H_
#define DAG_DAG_GRAPH_H_

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "dag/task_node.h"

namespace dag {

class DagGraph {
 public:
  bool AddNode(int id, long duration);
  bool AddEdge(int from, int to);

  bool ContainsNode(int id) const;
  std::size_t NodeCount() const;

  const std::vector<TaskNode>& Nodes() const;
  const TaskNode* GetNode(int id) const;
  std::vector<int> TopologicalOrder() const;

 private:
  int NodeIndex(int id) const;
  std::vector<TaskNode> nodes_;
  std::unordered_map<int, int> id_to_index_;
};

} 

#endif
