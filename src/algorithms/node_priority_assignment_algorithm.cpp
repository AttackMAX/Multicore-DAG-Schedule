#include "algorithms/node_priority_assignment_algorithm.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace algorithms {

CriticalPathMetrics NodePriorityAssignmentAlgorithm::ComputeCriticalPathMetrics(
    const dag::DagGraph& graph) const {
  CriticalPathMetrics metrics;
  const std::vector<int> topo = graph.TopologicalOrder();
  if (topo.size() != graph.NodeCount()) {
    return metrics;
  }

  std::unordered_map<int, std::vector<int>> predecessors;
  predecessors.reserve(graph.NodeCount());
  for (const dag::TaskNode& node : graph.Nodes()) {
    predecessors[node.id] = {};
  }
  for (const dag::TaskNode& node : graph.Nodes()) {
    for (int succ : node.outgoing_edges) {
      if (predecessors.find(succ) != predecessors.end()) {
        predecessors[succ].push_back(node.id);
      }
    }
  }

  for (int node_id : topo) {
    const dag::TaskNode* node = graph.GetNode(node_id);
    if (node == nullptr) {
      return metrics;
    }
    std::int64_t pred_max = 0;
    for (int pred : predecessors[node_id]) {
      const auto it = metrics.lf.find(pred);
      if (it != metrics.lf.end()) {
        pred_max = std::max(pred_max, it->second);
      }
    }
    metrics.lf[node_id] = node->duration + pred_max;
  }

  for (auto it = topo.rbegin(); it != topo.rend(); ++it) {
    const int node_id = *it;
    const dag::TaskNode* node = graph.GetNode(node_id);
    if (node == nullptr) {
      return metrics;
    }
    std::int64_t succ_max = 0;
    for (int succ : node->outgoing_edges) {
      const auto succ_it = metrics.lb.find(succ);
      if (succ_it != metrics.lb.end()) {
        succ_max = std::max(succ_max, succ_it->second);
      }
    }
    metrics.lb[node_id] = node->duration + succ_max;
  }

  for (const dag::TaskNode& node : graph.Nodes()) {
    metrics.longest_path[node.id] =
        metrics.lf[node.id] + metrics.lb[node.id] - node.duration;
    metrics.critical_path_length = std::max(
        metrics.critical_path_length, metrics.longest_path[node.id]);
  }
  metrics.valid = true;
  return metrics;
}

std::vector<int> NodePriorityAssignmentAlgorithm::AssignPrioritiesByCriticalPath(
    const dag::DagGraph& graph, const CriticalPathMetrics& metrics) const {
  std::vector<int> priorities;
  const std::vector<int> topo = graph.TopologicalOrder();
  if (!metrics.valid || topo.size() != graph.NodeCount()) {
    return priorities;
  }

  std::unordered_map<int, int> indegree;
  indegree.reserve(graph.NodeCount());
  for (const dag::TaskNode& node : graph.Nodes()) {
    indegree[node.id] = 0;
  }
  for (const dag::TaskNode& node : graph.Nodes()) {
    for (int succ : node.outgoing_edges) {
      auto it = indegree.find(succ);
      if (it != indegree.end()) {
        ++it->second;
      }
    }
  }

  std::unordered_set<int> ready_set;
  ready_set.reserve(graph.NodeCount());
  for (const auto& item : indegree) {
    if (item.second == 0) {
      ready_set.insert(item.first);
    }
  }

  priorities.reserve(graph.NodeCount());
  while (!ready_set.empty()) {
    int best_node = -1;
    std::int64_t best_weight = std::numeric_limits<std::int64_t>::min();
    for (int candidate : ready_set) {
      const auto weight_it = metrics.longest_path.find(candidate);
      if (weight_it == metrics.longest_path.end()) {
        continue;
      }
      const std::int64_t weight = weight_it->second;
      if (best_node < 0 || weight > best_weight ||
          (weight == best_weight && candidate < best_node)) {
        best_node = candidate;
        best_weight = weight;
      }
    }
    if (best_node < 0) {
      return {};
    }
    priorities.push_back(best_node);
    ready_set.erase(best_node);

    const dag::TaskNode* node = graph.GetNode(best_node);
    if (node == nullptr) {
      return {};
    }
    for (int succ : node->outgoing_edges) {
      auto indegree_it = indegree.find(succ);
      if (indegree_it == indegree.end()) {
        continue;
      }
      --indegree_it->second;
      if (indegree_it->second == 0) {
        ready_set.insert(succ);
      }
    }
  }

  if (priorities.size() != graph.NodeCount()) {
    return {};
  }
  return priorities;
}

ResponseTimeAnalysis NodePriorityAssignmentAlgorithm::AnalyzeResponseTime(
    const dag::DagGraph& graph, const std::vector<int>& priorities,
    int core_count) const {
  ResponseTimeAnalysis analysis;
  if (core_count <= 0 || priorities.size() != graph.NodeCount()) {
    return analysis;
  }
  const std::vector<int> topo = graph.TopologicalOrder();
  if (topo.size() != graph.NodeCount()) {
    return analysis;
  }

  std::unordered_map<int, int> priority_rank;
  priority_rank.reserve(priorities.size());
  for (std::size_t i = 0; i < priorities.size(); ++i) {
    priority_rank[priorities[i]] = static_cast<int>(i);
  }

  std::unordered_map<int, std::vector<int>> predecessors;
  predecessors.reserve(graph.NodeCount());
  for (const dag::TaskNode& node : graph.Nodes()) {
    predecessors[node.id] = {};
  }
  for (const dag::TaskNode& node : graph.Nodes()) {
    for (int succ : node.outgoing_edges) {
      if (predecessors.find(succ) != predecessors.end()) {
        predecessors[succ].push_back(node.id);
      }
    }
  }

  std::unordered_map<int, std::unordered_set<int>> ancestor_set;
  ancestor_set.reserve(graph.NodeCount());
  for (int node_id : topo) {
    std::unordered_set<int> current_ancestors;
    for (int pred : predecessors[node_id]) {
      current_ancestors.insert(pred);
      const auto pred_ancestors = ancestor_set.find(pred);
      if (pred_ancestors != ancestor_set.end()) {
        current_ancestors.insert(pred_ancestors->second.begin(),
                                 pred_ancestors->second.end());
      }
    }
    ancestor_set[node_id] = std::move(current_ancestors);
  }

  analysis.finish_time.reserve(graph.NodeCount());
  for (int node_id : topo) {
    const dag::TaskNode* node = graph.GetNode(node_id);
    if (node == nullptr) {
      return {};
    }
    double pred_finish_max = 0.0;
    for (int pred : predecessors[node_id]) {
      const auto pred_finish = analysis.finish_time.find(pred);
      if (pred_finish != analysis.finish_time.end()) {
        pred_finish_max = std::max(pred_finish_max, pred_finish->second);
      }
    }

    double interference_sum = 0.0;
    const int current_rank = priority_rank[node_id];
    for (const dag::TaskNode& candidate : graph.Nodes()) {
      if (candidate.id == node_id) {
        continue;
      }
      const int candidate_rank = priority_rank[candidate.id];
      if (candidate_rank >= current_rank) {
        continue;
      }
      if (ancestor_set[node_id].find(candidate.id) !=
          ancestor_set[node_id].end()) {
        continue;
      }
      interference_sum += static_cast<double>(candidate.duration);
    }

    const double finish_time =
        pred_finish_max + static_cast<double>(node->duration) +
        (interference_sum / static_cast<double>(core_count));
    analysis.finish_time[node_id] = finish_time;
    analysis.worst_response_time =
        std::max(analysis.worst_response_time, finish_time);
  }

  analysis.valid = true;
  return analysis;
}

}
