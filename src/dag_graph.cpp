#include "dag/dag_graph.h"

#include <queue>

namespace dag {

bool DagGraph::AddNode(int id, long duration) {
  if (ContainsNode(id)) {
    return false;
  }
  TaskNode node;
  node.id = id;
  node.duration = duration;
  id_to_index_[id] = static_cast<int>(nodes_.size());
  nodes_.push_back(node);
  return true;
}

bool DagGraph::AddEdge(int from, int to) {
  const int from_index = NodeIndex(from);
  const int to_index = NodeIndex(to);
  if (from_index < 0 || to_index < 0) {
    return false;
  }
  nodes_[from_index].outgoing_edges.push_back(to);
  return true;
}

bool DagGraph::ContainsNode(int id) const { return NodeIndex(id) >= 0; }

std::size_t DagGraph::NodeCount() const { return nodes_.size(); }

const std::vector<TaskNode>& DagGraph::Nodes() const { return nodes_; }

const TaskNode* DagGraph::GetNode(int id) const {
  const int index = NodeIndex(id);
  if (index < 0) {
    return nullptr;
  }
  return &nodes_[index];
}

std::vector<int> DagGraph::TopologicalOrder() const {
  std::vector<int> order;
  order.reserve(nodes_.size());
  std::vector<int> indegree(nodes_.size(), 0);

  for (const TaskNode& node : nodes_) {
    for (int to : node.outgoing_edges) {
      const int to_index = NodeIndex(to);
      if (to_index >= 0) {
        ++indegree[to_index];
      }
    }
  }

  std::queue<int> ready_indices;
  for (std::size_t i = 0; i < nodes_.size(); ++i) {
    if (indegree[i] == 0) {
      ready_indices.push(static_cast<int>(i));
    }
  }

  while (!ready_indices.empty()) {
    const int current_index = ready_indices.front();
    ready_indices.pop();
    order.push_back(nodes_[current_index].id);

    for (int to : nodes_[current_index].outgoing_edges) {
      const int to_index = NodeIndex(to);
      if (to_index >= 0) {
        --indegree[to_index];
        if (indegree[to_index] == 0) {
          ready_indices.push(to_index);
        }
      }
    }
  }

  if (order.size() != nodes_.size()) {
    return {};
  }
  return order;
}

int DagGraph::NodeIndex(int id) const {
  auto it = id_to_index_.find(id);
  return (it != id_to_index_.end()) ? it->second : -1;
}

}
