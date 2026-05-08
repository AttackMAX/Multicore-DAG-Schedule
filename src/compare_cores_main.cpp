#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "algorithms/dag_generator.h"
#include "algorithms/graham_response_time_analysis.h"
#include "algorithms/minimum_core_algorithm.h"
#include "algorithms/node_priority_assignment_algorithm.h"
#include "utils/json_utils.h"

namespace {

std::string GetArg(int argc, char* argv[], const std::string& flag,
                   const std::string& default_val) {
  for (int i = 1; i < argc - 1; ++i) {
    if (argv[i] == flag) {
      return argv[i + 1];
    }
  }
  return default_val;
}

}  // namespace

int main(int argc, char* argv[]) {
  double target_from = std::stod(GetArg(argc, argv, "--target-from", "0"));
  double target_to = std::stod(GetArg(argc, argv, "--target-to", "0"));
  double target_step = std::stod(GetArg(argc, argv, "--target-step", "1"));
  int max_cores = std::stoi(GetArg(argc, argv, "--max-cores", "256"));
  std::string dag_name = GetArg(argc, argv, "--dag", "random_20");
  std::string output_file = GetArg(argc, argv, "--output", "");

  std::ostream* out = &std::cout;
  std::ofstream fout;
  if (!output_file.empty()) {
    fout.open(output_file);
    if (!fout.is_open()) {
      std::cerr << "Cannot open output file: " << output_file << "\n";
      return 1;
    }
    out = &fout;
  }
  std::ostream& os = *out;

  algorithms::GeneratedDag generated;
  if (dag_name.find("chain_") == 0) {
    int n = std::stoi(dag_name.substr(6));
    generated = algorithms::DAGGenerator::Chain(n);
  } else if (dag_name.find("fork_join_") == 0) {
    int n = std::stoi(dag_name.substr(10));
    generated = algorithms::DAGGenerator::ForkJoin(n);
  } else if (dag_name.find("random_") == 0) {
    int n = std::stoi(dag_name.substr(7));
    generated = algorithms::DAGGenerator::RandomDAG(n, 0.3, 42);
  } else {
    std::cerr << "Unknown DAG name: " << dag_name << "\n";
    return 1;
  }

  // Auto-range: if no target range given, derive from critical path and m=1
  if (target_from == 0.0 && target_to == 0.0) {
    algorithms::NodePriorityAssignmentAlgorithm prio_algo;
    auto metrics = prio_algo.ComputeCriticalPathMetrics(generated.graph);
    if (!metrics.valid) {
      std::cerr << "Failed to compute critical path metrics\n";
      return 1;
    }
    target_from = static_cast<double>(metrics.critical_path_length);

    algorithms::GrahamResponseTimeAlgorithm graham_algo;
    auto probe = graham_algo.Analyze(generated.graph, 1);
    if (!probe.valid) {
      std::cerr << "Failed to probe response time\n";
      return 1;
    }
    target_to = probe.worst_response_time;
    target_step = (target_to - target_from) / 10.0;
    if (target_step <= 0.0) target_step = 1.0;
  }

  std::vector<double> target_values;
  for (double v = target_from; v <= target_to + target_step * 0.5;
       v += target_step) {
    target_values.push_back(v);
  }

  algorithms::MinimumCoreAlgorithm min_core_algo;

  os << "{\n";
  utils::WriteString(os, "dag_label", generated.label);
  os << ",\n";
  os << "  \"target_values\": [";
  {
    utils::Comma comma;
    for (double v : target_values) {
      comma.Write(os);
      os << v;
    }
  }
  os << "],\n";

  os << "  \"data_points\": [\n";
  utils::Comma point_comma;
  for (double target : target_values) {
    auto prio_result =
        min_core_algo.ComputeForPriority(generated.graph, target, max_cores);
    auto graham_result =
        min_core_algo.ComputeForGraham(generated.graph, target, max_cores);

    point_comma.Write(os);
    os << "    {\n";
    utils::WriteDouble(os, "target_response_time", target);
    os << ",\n";

    if (prio_result.feasible) {
      utils::WriteInt(os, "priority_min_cores", prio_result.min_cores);
      os << ",\n";
      utils::WriteDouble(os, "priority_actual_response_time",
                          prio_result.worst_response_time);
    } else {
      os << "      \"priority_min_cores\": null,\n";
      os << "      \"priority_actual_response_time\": null";
    }
    os << ",\n";

    if (graham_result.feasible) {
      utils::WriteInt(os, "graham_min_cores", graham_result.min_cores);
      os << ",\n";
      utils::WriteDouble(os, "graham_actual_response_time",
                          graham_result.worst_response_time);
    } else {
      os << "      \"graham_min_cores\": null,\n";
      os << "      \"graham_actual_response_time\": null";
    }
    os << ",\n";

    if (prio_result.feasible && graham_result.feasible &&
        graham_result.min_cores > 0) {
      double ratio = static_cast<double>(prio_result.min_cores) /
                     static_cast<double>(graham_result.min_cores);
      utils::WriteDouble(os, "core_ratio", ratio);
    } else {
      os << "      \"core_ratio\": null";
    }
    os << "\n    }";
  }

  os << "\n  ]\n}\n";

  return 0;
}
