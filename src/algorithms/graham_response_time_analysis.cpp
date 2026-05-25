#include "algorithms/graham_response_time_analysis.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace algorithms {

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

// ──── Analyze (bitset-based) ──────────────────────────────────────────────

GrahamResponseTimeAnalysis GrahamResponseTimeAlgorithm::Analyze(
    const dag::DagGraph& graph, int core_count) const {
  GrahamResponseTimeAnalysis analysis;
  if (core_count <= 0) {
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

  // ── duration by position & total work ──────────────────────────────
  std::vector<std::int64_t> dur(N);
  analysis.total_work = 0;
  for (int i = 0; i < N; ++i) {
    const dag::TaskNode* node = graph.GetNode(id_of[i]);
    dur[i] = node ? node->duration : 0;
    analysis.total_work += dur[i];
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

  // ── ancestors bitsets + lf (forward pass) ───────────────────────────
  std::vector<std::int64_t> lf(N);
  std::vector<BitSet> ancestors(N, BitSet(N));
  for (int i = 0; i < N; ++i) {
    std::int64_t pred_max = 0;
    for (int p : pred[i]) {
      ancestors[i].Set(p);
      ancestors[i].OrWith(ancestors[p]);
      pred_max = std::max(pred_max, lf[p]);
    }
    lf[i] = dur[i] + pred_max;
  }

  analysis.critical_path_length = 0;
  for (std::int64_t v : lf) {
    analysis.critical_path_length =
        std::max(analysis.critical_path_length, v);
  }

  double L = static_cast<double>(analysis.critical_path_length);
  double W = static_cast<double>(analysis.total_work);
  double m = static_cast<double>(core_count);
  analysis.graham_makespan_bound = L + (W - L) / m;

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

  // ── I(v) = incomparable set (no priority filter for Graham) ─────────
  // I(v) = all \ {v} \ ancestors[v] \ descendants[v]
  std::vector<BitSet> I_v(N, BitSet(N));
  for (int v = 0; v < N; ++v) {
    I_v[v].SetAll();
    I_v[v].ClearBit(v);
    I_v[v].AndNot(ancestors[v]);
    I_v[v].AndNot(descendants[v]);
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

  analysis.finish_time.reserve(N);

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

}  // namespace algorithms
