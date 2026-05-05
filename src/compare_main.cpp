#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "algorithms/dag_generator.h"
#include "algorithms/graham_response_time_analysis.h"
#include "algorithms/node_priority_assignment_algorithm.h"
#include "dag/dag_graph.h"
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
  std::string param_name = GetArg(argc, argv, "--param", "cores");
  int from = std::stoi(GetArg(argc, argv, "--from", "1"));
  int to = std::stoi(GetArg(argc, argv, "--to", "8"));
  int step = std::stoi(GetArg(argc, argv, "--step", "1"));
  int fixed_cores = std::stoi(GetArg(argc, argv, "--cores", "4"));
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

  algorithms::NodePriorityAssignmentAlgorithm prio_algo;
  algorithms::GrahamResponseTimeAlgorithm graham_algo;

  // Generate the DAG (or re-generate per iteration for --param nodes)
  algorithms::GeneratedDag base_dag;

  if (param_name == "nodes") {
    base_dag.label = dag_name;
  } else {
    if (dag_name.find("chain_") == 0) {
      int n = std::stoi(dag_name.substr(6));
      base_dag = algorithms::DAGGenerator::Chain(n);
    } else if (dag_name.find("fork_join_") == 0) {
      int n = std::stoi(dag_name.substr(10));
      base_dag = algorithms::DAGGenerator::ForkJoin(n);
    } else if (dag_name.find("random_") == 0) {
      int n = std::stoi(dag_name.substr(7));
      base_dag = algorithms::DAGGenerator::RandomDAG(n, 0.3, 42);
    } else {
      std::cerr << "Unknown DAG name: " << dag_name << "\n";
      return 1;
    }
  }

  std::vector<int> param_values;
  for (int v = from; v <= to; v += step) {
    param_values.push_back(v);
  }

  // Header
  os << "{\n";
  utils::WriteString(os, "param_name", param_name);
  os << ",\n";
  os << "  \"param_values\": [";
  {
    utils::Comma comma;
    for (int v : param_values) {
      comma.Write(os);
      os << v;
    }
  }
  os << "],\n";
  utils::WriteString(os, "dag_label", base_dag.label);
  os << ",\n";
  utils::WriteInt(os, "fixed_cores",
                  (param_name == "nodes") ? fixed_cores : 0);
  os << ",\n";

  // Compute per-point
  os << "  \"data_points\": [\n";
  utils::Comma point_comma;
  for (std::size_t pi = 0; pi < param_values.size(); ++pi) {
    int param_val = param_values[pi];
    int core_count = (param_name == "cores") ? param_val : fixed_cores;

    dag::DagGraph graph;
    if (param_name == "nodes") {
      auto gen = algorithms::DAGGenerator::RandomDAG(param_val, 0.3, 42 + pi);
      graph = gen.graph;
    } else {
      graph = base_dag.graph;
    }

    auto metrics = prio_algo.ComputeCriticalPathMetrics(graph);
    if (!metrics.valid) {
      std::cerr << "Critical path metrics failed at param=" << param_val
                << "\n";
      return 1;
    }

    auto priorities = prio_algo.AssignPrioritiesByCriticalPath(graph, metrics);
    if (priorities.empty()) {
      std::cerr << "Priority assignment failed at param=" << param_val << "\n";
      return 1;
    }

    auto prio_response =
        prio_algo.AnalyzeResponseTime(graph, priorities, core_count);
    if (!prio_response.valid) {
      std::cerr << "Priority response analysis failed at param=" << param_val
                << "\n";
      return 1;
    }

    auto graham_response = graham_algo.Analyze(graph, core_count);
    if (!graham_response.valid) {
      std::cerr << "Graham analysis failed at param=" << param_val << "\n";
      return 1;
    }

    double tightening = 1.0;
    if (graham_response.worst_response_time > 0.0) {
      tightening = prio_response.worst_response_time /
                   graham_response.worst_response_time;
    }

    point_comma.Write(os);
    os << "    {\n";
    utils::WriteInt(os, "param_value", param_val);
    os << ",\n";
    utils::WriteDouble(os, "priority_worst_response_time",
                        prio_response.worst_response_time);
    os << ",\n";
    utils::WriteDouble(os, "graham_worst_response_time",
                        graham_response.worst_response_time);
    os << ",\n";
    utils::WriteDouble(os, "graham_makespan_bound",
                        graham_response.graham_makespan_bound);
    os << ",\n";
    utils::WriteDouble(os, "tightening_ratio", tightening);
    os << ",\n";
    utils::WriteInt64(os, "total_work", graham_response.total_work);
    os << ",\n";
    utils::WriteInt64(os, "critical_path_length",
                       graham_response.critical_path_length);
    os << "\n    }";
  }

  os << "\n  ]\n}\n";

  return 0;
}
