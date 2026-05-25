#include "algorithms/node_priority_assignment_algorithm.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

// ──── Compact bitset helpers (anonymous namespace) ────────────────────────

namespace {

class BitSet {
 public:
  explicit BitSet(int n) : n_(n), bits_((n + 63) / 64, 0) {}

  void Set(int pos) { bits_[pos >> 6] |= (std::uint64_t{1} << (pos & 63)); }
  bool Test(int pos) const {
    return (bits_[pos >> 6] >> (pos & 63)) & std::uint64_t{1};
  }
  void ClearBit(int pos) {
    bits_[pos >> 6] &= ~(std::uint64_t{1} << (pos & 63));
  }
  void SetAll() {
    for (auto& w : bits_) w = ~std::uint64_t{0};
    if (n_ % 64) bits_.back() &= (std::uint64_t{1} << (n_ % 64)) - 1;
  }
  void OrWith(const BitSet& o) {
    for (std::size_t i = 0; i < bits_.size(); ++i) bits_[i] |= o.bits_[i];
  }
  void AndNot(const BitSet& o) {
    for (std::size_t i = 0; i < bits_.size(); ++i) bits_[i] &= ~o.bits_[i];
  }
  void AndWith(const BitSet& o) {
    for (std::size_t i = 0; i < bits_.size(); ++i) bits_[i] &= o.bits_[i];
  }
  const std::vector<std::uint64_t>& Words() const { return bits_; }
  int Size() const { return n_; }

 private:
  int n_;
  std::vector<std::uint64_t> bits_;
};

std::int64_t BitsetVol(const BitSet& bs,
                       const std::vector<std::int64_t>& dur) {
  std::int64_t total = 0;
  const auto& words = bs.Words();
  const int n = bs.Size();
  for (std::size_t w = 0; w < words.size(); ++w) {
    std::uint64_t m = words[w];
    while (m) {
      int bit = __builtin_ctzll(m);
      int pos = static_cast<int>(w * 64 + bit);
      if (pos < n) total += dur[pos];
      m &= m - 1;
    }
  }
  return total;
}

std::int64_t IntersectionVol(const BitSet& a, const BitSet& b,
                             const std::vector<std::int64_t>& dur) {
  std::int64_t total = 0;
  const auto& wa = a.Words();
  const auto& wb = b.Words();
  const int n = a.Size();
  for (std::size_t w = 0; w < wa.size(); ++w) {
    std::uint64_t both = wa[w] & wb[w];
    while (both) {
      int bit = __builtin_ctzll(both);
      int pos = static_cast<int>(w * 64 + bit);
      if (pos < n) total += dur[pos];
      both &= both - 1;
    }
  }
  return total;
}

}  // namespace

// ──── AnalyzeResponseTime (bitset-based) ──────────────────────────────────

ResponseTimeAnalysis NodePriorityAssignmentAlgorithm::AnalyzeResponseTime(
    const dag::DagGraph& graph, const std::vector<int>& priorities,
    int core_count) const {
  ResponseTimeAnalysis analysis;
  if (core_count <= 0 || priorities.size() != graph.NodeCount()) {
    return analysis;
  }
  const std::vector<int> topo_id = graph.TopologicalOrder();
  const int N = static_cast<int>(graph.NodeCount());
  if (topo_id.size() != static_cast<std::size_t>(N)) {
    return analysis;
  }

  // ── compact mapping: node ID ↔ position 0..N-1 ──────────────────────
  std::unordered_map<int, int> pos_of;
  pos_of.reserve(N);
  std::vector<int> id_of(N);
  for (int i = 0; i < N; ++i) {
    int id = topo_id[i];
    pos_of[id] = i;
    id_of[i] = id;
  }

  // ── duration & priority rank by position ────────────────────────────
  std::vector<std::int64_t> dur(N);
  std::vector<int> prio_rank(N);
  for (int i = 0; i < N; ++i) {
    const dag::TaskNode* node = graph.GetNode(id_of[i]);
    dur[i] = node ? node->duration : 0;
    prio_rank[i] = N;  // sentinel: lowest priority
  }
  for (std::size_t i = 0; i < priorities.size(); ++i) {
    auto it = pos_of.find(priorities[i]);
    if (it != pos_of.end()) prio_rank[it->second] = static_cast<int>(i);
  }

  // ── predecessors by position ───────────────────────────────────────
  std::vector<std::vector<int>> pred(N);
  for (int i = 0; i < N; ++i) {
    const dag::TaskNode* node = graph.GetNode(id_of[i]);
    if (!node) return analysis;
    for (int succ : node->outgoing_edges) {
      auto it = pos_of.find(succ);
      if (it != pos_of.end()) pred[it->second].push_back(i);
    }
  }

  // ── ancestors bitsets (forward pass) ────────────────────────────────
  std::vector<BitSet> ancestors(N, BitSet(N));
  for (int i = 0; i < N; ++i) {
    for (int p : pred[i]) {
      ancestors[i].Set(p);
      ancestors[i].OrWith(ancestors[p]);
    }
  }

  // ── descendants bitsets (backward pass) ─────────────────────────────
  std::vector<BitSet> descendants(N, BitSet(N));
  for (int i = N - 1; i >= 0; --i) {
    int id = id_of[i];
    const dag::TaskNode* node = graph.GetNode(id);
    if (!node) return analysis;
    for (int succ : node->outgoing_edges) {
      auto it = pos_of.find(succ);
      if (it == pos_of.end()) continue;
      int sp = it->second;
      descendants[i].Set(sp);
      descendants[i].OrWith(descendants[sp]);
    }
  }

  // ── precompute priority masks: mask[r] = nodes with rank < r ────────
  std::vector<BitSet> higher_prio_mask(N + 1, BitSet(N));
  std::vector<int> rank_to_pos(N);
  for (int i = 0; i < N; ++i) rank_to_pos[prio_rank[i]] = i;
  for (int r = 1; r <= N; ++r) {
    higher_prio_mask[r] = higher_prio_mask[r - 1];
    higher_prio_mask[r].Set(rank_to_pos[r - 1]);
  }

  // ── I(v) bitsets ───────────────────────────────────────────────────
  // I(v) = {c : prio_rank[c] < prio_rank[v], c ∉ ancestors[v],
  //               c ∉ descendants[v], c ≠ v}
  std::vector<BitSet> I_v(N, BitSet(N));
  for (int v = 0; v < N; ++v) {
    I_v[v].SetAll();
    I_v[v].ClearBit(v);
    I_v[v].AndNot(ancestors[v]);
    I_v[v].AndNot(descendants[v]);
    I_v[v].AndWith(higher_prio_mask[prio_rank[v]]);
  }

  // ── precompute vol of each I(v) ────────────────────────────────────
  std::vector<std::int64_t> vol_I(N);
  for (int i = 0; i < N; ++i) {
    vol_I[i] = BitsetVol(I_v[i], dur);
  }

  // ── DP ─────────────────────────────────────────────────────────────
  std::vector<std::int64_t> path_len(N, 0);
  std::vector<BitSet> path_I(N, BitSet(N));
  std::vector<std::int64_t> vol_path_I(N, 0);

  double m = static_cast<double>(core_count);

  for (int i = 0; i < N; ++i) {
    int id = id_of[i];

    if (pred[i].empty()) {
      path_len[i] = dur[i];
      vol_path_I[i] = 0;
    } else {
      double best_R = -1.0;
      int best_u = -1;
      for (int u : pred[i]) {
        std::int64_t inter_vol = IntersectionVol(path_I[u], I_v[i], dur);
        std::int64_t cand_vol = vol_path_I[u] + vol_I[i] - inter_vol;
        double cand_R = static_cast<double>(path_len[u]) +
                        static_cast<double>(dur[i]) +
                        static_cast<double>(cand_vol) / m;
        if (cand_R > best_R) {
          best_R = cand_R;
          best_u = u;
        }
      }
      path_len[i] = path_len[best_u] + dur[i];
      path_I[i] = path_I[best_u];
      path_I[i].OrWith(I_v[i]);
      vol_path_I[i] = BitsetVol(path_I[i], dur);
    }

    double r = static_cast<double>(path_len[i]) +
               static_cast<double>(vol_path_I[i]) / m;
    analysis.finish_time[id] = r;
    analysis.worst_response_time = std::max(analysis.worst_response_time, r);
  }

  analysis.valid = true;
  return analysis;
}

}
