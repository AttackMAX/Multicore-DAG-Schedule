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

  std::unordered_map<int, std::unordered_set<int>> ancestors;
  ancestors.reserve(graph.NodeCount());
  for (int node_id : topo) {
    std::unordered_set<int> anc;
    for (int pred : predecessors[node_id]) {
      anc.insert(pred);
      const auto& pred_anc = ancestors[pred];
      anc.insert(pred_anc.begin(), pred_anc.end());
    }
    ancestors[node_id] = std::move(anc);
  }

  std::unordered_map<int, std::unordered_set<int>> descendants;
  descendants.reserve(graph.NodeCount());
  for (auto it = topo.rbegin(); it != topo.rend(); ++it) {
    int node_id = *it;
    const dag::TaskNode* node = graph.GetNode(node_id);
    if (node == nullptr) {
      return analysis;
    }
    std::unordered_set<int> desc;
    for (int succ : node->outgoing_edges) {
      desc.insert(succ);
      const auto& succ_desc = descendants[succ];
      desc.insert(succ_desc.begin(), succ_desc.end());
    }
    descendants[node_id] = std::move(desc);
  }

  // Pre-compute I(v) for each vertex
  std::unordered_map<int, std::unordered_set<int>> I_v;
  I_v.reserve(graph.NodeCount());
  for (const dag::TaskNode& node : graph.Nodes()) {
    std::unordered_set<int> iv;
    for (const dag::TaskNode& cand : graph.Nodes()) {
      if (cand.id == node.id) continue;
      if (priority_rank[cand.id] >= priority_rank[node.id]) continue;
      if (ancestors[node.id].find(cand.id) != ancestors[node.id].end())
        continue;
      if (descendants[node.id].find(cand.id) != descendants[node.id].end())
        continue;
      iv.insert(cand.id);
    }
    I_v[node.id] = std::move(iv);
  }

  // Duration lookup helper
  auto vol_of = [&](const std::unordered_set<int>& ids) -> std::int64_t {
    std::int64_t total = 0;
    for (int id : ids) {
      const dag::TaskNode* n = graph.GetNode(id);
      if (n != nullptr) total += n->duration;
    }
    return total;
  };

  // DP: path_len, path_I, and R for each vertex
  std::unordered_map<int, std::int64_t> path_len;
  std::unordered_map<int, std::unordered_set<int>> path_I;
  path_len.reserve(graph.NodeCount());
  path_I.reserve(graph.NodeCount());
  analysis.finish_time.reserve(graph.NodeCount());

  for (int node_id : topo) {
    const dag::TaskNode* node = graph.GetNode(node_id);
    if (node == nullptr) {
      return {};
    }

    if (predecessors[node_id].empty()) {
      path_len[node_id] = node->duration;
      path_I[node_id] = {};
    } else {
      double best_R = -1.0;
      int best_u = -1;
      for (int u : predecessors[node_id]) {
        std::unordered_set<int> cand_I = path_I[u];
        const auto& iv = I_v[node_id];
        cand_I.insert(iv.begin(), iv.end());

        std::int64_t cand_vol = vol_of(cand_I);
        double cand_R = static_cast<double>(path_len[u]) +
                        static_cast<double>(node->duration) +
                        static_cast<double>(cand_vol) /
                            static_cast<double>(core_count);
        if (cand_R > best_R) {
          best_R = cand_R;
          best_u = u;
        }
      }
      path_len[node_id] = path_len[best_u] + node->duration;
      path_I[node_id] = path_I[best_u];
      path_I[node_id].insert(I_v[node_id].begin(), I_v[node_id].end());
    }

    std::int64_t total_I_vol = vol_of(path_I[node_id]);
    double r = static_cast<double>(path_len[node_id]) +
               static_cast<double>(total_I_vol) /
                   static_cast<double>(core_count);
    analysis.finish_time[node_id] = r;
    analysis.worst_response_time = std::max(analysis.worst_response_time, r);
  }

  analysis.valid = true;
  return analysis;
}

}
