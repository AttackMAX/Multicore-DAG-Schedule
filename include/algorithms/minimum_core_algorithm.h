#ifndef ALGORITHMS_MINIMUM_CORE_ALGORITHM_H_
#define ALGORITHMS_MINIMUM_CORE_ALGORITHM_H_

#include "dag/dag_graph.h"

namespace algorithms {

struct MinimumCoreResult {
  int min_cores = 0;
  double worst_response_time = 0.0;
  bool feasible = false;
};

class MinimumCoreAlgorithm {
 public:
  MinimumCoreResult ComputeForPriority(const dag::DagGraph& graph,
                                       double target_response_time,
                                       int max_cores = 256) const;

  MinimumCoreResult ComputeForGraham(const dag::DagGraph& graph,
                                     double target_response_time,
                                     int max_cores = 256) const;
};

}  // namespace algorithms

#endif
