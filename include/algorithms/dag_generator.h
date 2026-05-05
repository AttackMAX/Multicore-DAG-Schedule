#ifndef ALGORITHMS_DAG_GENERATOR_H_
#define ALGORITHMS_DAG_GENERATOR_H_

#include <string>
#include <vector>

#include "dag/dag_graph.h"

namespace algorithms {

struct GeneratedDag {
  dag::DagGraph graph;
  std::string label;
};

class DAGGenerator {
 public:
  static GeneratedDag Chain(int n, std::int64_t duration = 1);
  static GeneratedDag ForkJoin(int n, std::int64_t duration = 1);
  static GeneratedDag RandomDAG(int n, double edge_prob,
                                 unsigned seed = 42,
                                 std::int64_t min_dur = 1,
                                 std::int64_t max_dur = 10);
  static std::vector<GeneratedDag> GalleryExamples();
};

}  // namespace algorithms

#endif
