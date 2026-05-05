#include "algorithms/graham_response_time_analysis.h"

#include <algorithm>
#include <unordered_set>

namespace algorithms {

GrahamResponseTimeAnalysis GrahamResponseTimeAlgorithm::Analyze(
    const dag::DagGraph& graph, int core_count) const {
  GrahamResponseTimeAnalysis analysis;
  if (core_count <= 0) {
    return analysis;
  }

  const std::vector<int> topo = graph.TopologicalOrder();
  if (topo.size() != graph.NodeCount()) {
    return analysis;
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

  std::unordered_map<int, std::int64_t> lf;
  analysis.total_work = 0;
  for (int node_id : topo) {
    const dag::TaskNode* node = graph.GetNode(node_id);
    if (node == nullptr) {
      return analysis;
    }
    analysis.total_work += node->duration;
    std::int64_t pred_max = 0;
    for (int pred : predecessors[node_id]) {
      pred_max = std::max(pred_max, lf[pred]);
    }
    lf[node_id] = node->duration + pred_max;
  }

  analysis.critical_path_length = 0;
  for (const auto& item : lf) {
    analysis.critical_path_length =
        std::max(analysis.critical_path_length, item.second);
  }

  double L = static_cast<double>(analysis.critical_path_length);
  double W = static_cast<double>(analysis.total_work);
  double m = static_cast<double>(core_count);
  analysis.graham_makespan_bound = L + (W - L) / m;

  // Pre-compute I(v) = incomparable set for each vertex
  std::unordered_map<int, std::unordered_set<int>> I_v;
  I_v.reserve(graph.NodeCount());
  for (const dag::TaskNode& node : graph.Nodes()) {
    std::unordered_set<int> iv;
    const auto& anc = ancestors[node.id];
    const auto& desc = descendants[node.id];
    for (const dag::TaskNode& cand : graph.Nodes()) {
      if (cand.id == node.id) continue;
      if (anc.find(cand.id) != anc.end()) continue;
      if (desc.find(cand.id) != desc.end()) continue;
      iv.insert(cand.id);
    }
    I_v[node.id] = std::move(iv);
  }

  auto vol_of = [&](const std::unordered_set<int>& ids) -> std::int64_t {
    std::int64_t total = 0;
    for (int id : ids) {
      const dag::TaskNode* n = graph.GetNode(id);
      if (n != nullptr) total += n->duration;
    }
    return total;
  };

  // DP with union-based interference sets
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
                        static_cast<double>(cand_vol) / m;
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
               static_cast<double>(total_I_vol) / m;
    analysis.finish_time[node_id] = r;
    analysis.worst_response_time = std::max(analysis.worst_response_time, r);
  }

  analysis.valid = true;
  return analysis;
}

}  // namespace algorithms
