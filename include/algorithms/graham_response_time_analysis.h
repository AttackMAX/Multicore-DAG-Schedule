#ifndef ALGORITHMS_GRAHAM_RESPONSE_TIME_ANALYSIS_H_
#define ALGORITHMS_GRAHAM_RESPONSE_TIME_ANALYSIS_H_

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "dag/dag_graph.h"

namespace algorithms {

struct GrahamResponseTimeAnalysis {
  std::unordered_map<int, double> finish_time;
  double worst_response_time = 0.0;
  double graham_makespan_bound = 0.0;
  std::int64_t critical_path_length = 0;
  std::int64_t total_work = 0;
  bool valid = false;
};

class GrahamResponseTimeAlgorithm {
 public:
  GrahamResponseTimeAnalysis Analyze(const dag::DagGraph& graph,
                                      int core_count) const;
};

}  // namespace algorithms

#endif
