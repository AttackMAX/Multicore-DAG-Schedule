#include "algorithms/dag_generator.h"

#include <cstdlib>
#include <ctime>
#include <random>

#include "io/stg_reader.h"

namespace algorithms {

GeneratedDag DAGGenerator::Chain(int n, std::int64_t duration) {
  GeneratedDag dag;
  dag.label = "chain_" + std::to_string(n);
  for (int i = 1; i <= n; ++i) {
    dag.graph.AddNode(i, duration);
  }
  for (int i = 1; i < n; ++i) {
    dag.graph.AddEdge(i, i + 1);
  }
  return dag;
}

GeneratedDag DAGGenerator::ForkJoin(int n, std::int64_t duration) {
  GeneratedDag dag;
  dag.label = "fork_join_" + std::to_string(n);
  if (n < 3) {
    return dag;
  }
  dag.graph.AddNode(1, duration);
  dag.graph.AddNode(n, duration);
  for (int i = 2; i < n; ++i) {
    dag.graph.AddNode(i, duration);
    dag.graph.AddEdge(1, i);
    dag.graph.AddEdge(i, n);
  }
  return dag;
}

GeneratedDag DAGGenerator::RandomDAG(int n, double edge_prob,
                                      unsigned seed,
                                      std::int64_t min_dur,
                                      std::int64_t max_dur) {
  GeneratedDag dag;
  dag.label = "random_" + std::to_string(n);
  std::mt19937 rng(seed);
  std::uniform_real_distribution<double> edge_dist(0.0, 1.0);
  std::uniform_int_distribution<std::int64_t> dur_dist(min_dur, max_dur);

  for (int i = 1; i <= n; ++i) {
    dag.graph.AddNode(i, dur_dist(rng));
  }
  for (int i = 1; i <= n; ++i) {
    for (int j = i + 1; j <= n; ++j) {
      if (edge_dist(rng) < edge_prob) {
        dag.graph.AddEdge(i, j);
      }
    }
  }
  return dag;
}

namespace {
std::string ResolveStgPath(const std::string& path) {
  if (path.empty() || path[0] == '/') return path;
  const char* dir = std::getenv("BUILD_WORKING_DIRECTORY");
  if (!dir) return path;
  return std::string(dir) + "/" + path;
}
}  // namespace

GeneratedDag DAGGenerator::Resolve(const std::string& dag_name) {
  if (dag_name.find("stg:") == 0) {
    std::string path = ResolveStgPath(dag_name.substr(4));
    GeneratedDag dag;
    dag.graph = io::StgReader::Read(path);
    dag.label = "stg:" + path;
    return dag;
  }
  if (dag_name.find("chain_") == 0) {
    int n = std::stoi(dag_name.substr(6));
    return Chain(n);
  }
  if (dag_name.find("fork_join_") == 0) {
    int n = std::stoi(dag_name.substr(10));
    return ForkJoin(n);
  }
  if (dag_name.find("random_") == 0) {
    int n = std::stoi(dag_name.substr(7));
    return RandomDAG(n, 0.3, 42);
  }
  return {};
}

std::vector<GeneratedDag> DAGGenerator::GalleryExamples() {
  return {
      Chain(10),
      ForkJoin(8),
      RandomDAG(12, 0.3, 42),
      RandomDAG(20, 0.25, 123),
  };
}

}  // namespace algorithms
