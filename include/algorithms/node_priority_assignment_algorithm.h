#ifndef ALGORITHMS_NODE_PRIORITY_ASSIGNMENT_ALGORITHM_H_
#define ALGORITHMS_NODE_PRIORITY_ASSIGNMENT_ALGORITHM_H_

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "dag/dag_graph.h"

namespace algorithms {

struct CriticalPathMetrics {
  std::unordered_map<int, std::int64_t> lf;
  std::unordered_map<int, std::int64_t> lb;
  std::unordered_map<int, std::int64_t> longest_path;
  std::int64_t critical_path_length = 0;
  bool valid = false;
};

struct ResponseTimeAnalysis {
  std::unordered_map<int, double> finish_time;
  double worst_response_time = 0.0;
  bool valid = false;
};

class NodePriorityAssignmentAlgorithm {
 public:
  CriticalPathMetrics ComputeCriticalPathMetrics(const dag::DagGraph& graph) const;
  std::vector<int> AssignPrioritiesByCriticalPath(
      const dag::DagGraph& graph, const CriticalPathMetrics& metrics) const;
  ResponseTimeAnalysis AnalyzeResponseTime(const dag::DagGraph& graph,
                                           const std::vector<int>& priorities,
                                           int core_count) const;
};

}

#endif
