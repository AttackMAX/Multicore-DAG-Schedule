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

  analysis.finish_time.reserve(graph.NodeCount());
  for (int node_id : topo) {
    const dag::TaskNode* node = graph.GetNode(node_id);
    if (node == nullptr) {
      return {};
    }

    double pred_finish_max = 0.0;
    for (int pred : predecessors[node_id]) {
      pred_finish_max =
          std::max(pred_finish_max, analysis.finish_time[pred]);
    }

    const auto& anc = ancestors[node_id];
    const auto& desc = descendants[node_id];
    double interference = 0.0;
    for (const dag::TaskNode& candidate : graph.Nodes()) {
      if (candidate.id == node_id) {
        continue;
      }
      if (anc.find(candidate.id) != anc.end()) {
        continue;
      }
      if (desc.find(candidate.id) != desc.end()) {
        continue;
      }
      interference += static_cast<double>(candidate.duration);
    }

    double finish_time = pred_finish_max + static_cast<double>(node->duration) +
                         interference / m;
    analysis.finish_time[node_id] = finish_time;
    analysis.worst_response_time =
        std::max(analysis.worst_response_time, finish_time);
  }

  analysis.valid = true;
  return analysis;
}

}  // namespace algorithms
