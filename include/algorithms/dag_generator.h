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
  // Resolves a DAG name string to a GeneratedDag.
  // Supported formats:
  //   chain_N      — chain of N nodes
  //   fork_join_N  — fork-join with N nodes
  //   random_N     — random DAG with N nodes, edge_prob=0.3, seed=42
  //   stg:<path>   — STG file at the given path (dummy nodes stripped)
  static GeneratedDag Resolve(const std::string& dag_name);

  static std::vector<GeneratedDag> GalleryExamples();
};

}  // namespace algorithms

#endif
