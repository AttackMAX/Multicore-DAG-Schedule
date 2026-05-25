#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "algorithms/dag_generator.h"
#include "algorithms/graham_response_time_analysis.h"
#include "algorithms/minimum_core_algorithm.h"
#include "algorithms/node_priority_assignment_algorithm.h"
#include "dag/dag_graph.h"
#include "utils/json_utils.h"

namespace {

// Resolve output path: if relative, prepend BUILD_WORKING_DIRECTORY so
// bazel run saves files to the user's CWD instead of the sandbox.
std::string ResolveOutputPath(const std::string& path) {
  if (path.empty() || path[0] == '/') return path;
  const char* dir = std::getenv("BUILD_WORKING_DIRECTORY");
  if (!dir) return path;
  return std::string(dir) + "/" + path;
}

std::string Trim(const std::string& s) {
  std::size_t b = 0;
  while (b < s.size() && (s[b] == ' ' || s[b] == '\t')) ++b;
  std::size_t e = s.size();
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
  return s.substr(b, e - b);
}

bool input_closed = false;

std::string PromptStr(const std::string& label, const std::string& dflt = "") {
  if (input_closed) return dflt;
  std::cout << label;
  if (!dflt.empty()) std::cout << " [" << dflt << "]";
  std::cout << ": ";
  std::string line;
  if (!std::getline(std::cin, line)) {
    input_closed = true;
    std::cout << "(EOF)\n";
    return dflt;
  }
  line = Trim(line);
  return line.empty() ? dflt : line;
}

int PromptInt(const std::string& label, int dflt) {
  while (!input_closed) {
    std::string raw = PromptStr(label, std::to_string(dflt));
    try {
      return std::stoi(raw);
    } catch (...) {
      std::cout << "  输入非法，请重试。\n";
    }
  }
  return dflt;
}

double PromptDouble(const std::string& label, double dflt) {
  while (!input_closed) {
    std::string raw = PromptStr(label, std::to_string(dflt));
    try {
      return std::stod(raw);
    } catch (...) {
      std::cout << "  输入非法，请重试。\n";
    }
  }
  return dflt;
}

void PressEnter() {
  std::cout << "\n[回车继续] ";
  std::string line;
  std::getline(std::cin, line);
  std::cout << "\n";
}

dag::DagGraph LoadDag(const std::string& name) {
  auto gen = algorithms::DAGGenerator::Resolve(name);
  if (gen.graph.NodeCount() == 0) {
    std::cout << "  加载 DAG 失败: " << name << "\n";
    return {};
  }
  std::cout << "  已加载: " << gen.label << " (" << gen.graph.NodeCount()
            << " 个节点)\n";
  return gen.graph;
}

void RunSingleAnalysis() {
  std::cout << "\n--- 单 DAG 分析 ---\n\n";
  std::cout << "DAG 格式: chain_N, fork_join_N, random_N, stg:<路径>\n";
  std::string dag_name = PromptStr("DAG 名称", "chain_10");
  int cores = PromptInt("核心数", 2);

  dag::DagGraph graph = LoadDag(dag_name);
  if (graph.NodeCount() == 0) {
    PressEnter();
    return;
  }

  algorithms::NodePriorityAssignmentAlgorithm prio;
  auto metrics = prio.ComputeCriticalPathMetrics(graph);
  if (!metrics.valid) {
    std::cout << "  关键路径指标计算失败。\n";
    PressEnter();
    return;
  }

  auto prios = prio.AssignPrioritiesByCriticalPath(graph, metrics);
  if (prios.empty()) {
    std::cout << "  优先级分配失败。\n";
    PressEnter();
    return;
  }

  auto pr = prio.AnalyzeResponseTime(graph, prios, cores);
  if (!pr.valid) {
    std::cout << "  优先级响应时间分析失败。\n";
    PressEnter();
    return;
  }

  algorithms::GrahamResponseTimeAlgorithm graham;
  auto gr = graham.Analyze(graph, cores);
  if (!gr.valid) {
    std::cout << "  Graham 分析失败。\n";
    PressEnter();
    return;
  }

  std::cout << "\n=== DAG 信息 ===\n";
  std::cout << "标签: " << dag_name << "\n";
  std::cout << "节点数: " << graph.NodeCount() << "\n";
  std::cout << "关键路径长度 (L): " << metrics.critical_path_length << "\n";
  std::cout << "总工作量 (W): " << gr.total_work << "\n";
  std::cout << "核心数 (m): " << cores << "\n";

  std::cout << "\n=== 基于优先级的调度 ===\n";
  std::cout << "优先级顺序:";
  for (int id : prios) std::cout << " " << id;
  std::cout << "\n";
  std::cout << "最坏响应时间: " << pr.worst_response_time << "\n";
  int col = 0;
  for (int id : graph.TopologicalOrder()) {
    std::cout << "  f_prio(" << id << ") = " << pr.finish_time.at(id);
    if (++col % 6 == 0) std::cout << "\n";
  }
  if (col % 6 != 0) std::cout << "\n";

  std::cout << "\n=== Graham 界（任意调度） ===\n";
  std::cout << "Makespan 界 L+(W-L)/m: " << gr.graham_makespan_bound << "\n";
  std::cout << "最坏响应时间: " << gr.worst_response_time << "\n";
  col = 0;
  for (int id : graph.TopologicalOrder()) {
    std::cout << "  f_grm(" << id << ") = " << gr.finish_time.at(id);
    if (++col % 6 == 0) std::cout << "\n";
  }
  if (col % 6 != 0) std::cout << "\n";

  std::cout << "\n=== 对比 ===\n";
  if (pr.worst_response_time <= gr.worst_response_time) {
    std::cout << "紧化比: "
              << (pr.worst_response_time / gr.worst_response_time) << "\n";
  } else {
    std::cout << "错误: 优先级界超出 Graham 界。\n";
  }

  std::string outfile = PromptStr("\n保存到文件（留空跳过）", "");
  if (!outfile.empty()) {
    std::ofstream f(ResolveOutputPath(outfile));
    if (f.is_open()) {
      f << "=== DAG 信息 ===\n";
      f << "标签: " << dag_name << "\n";
      f << "节点数: " << graph.NodeCount() << "\n";
      f << "关键路径长度 (L): " << metrics.critical_path_length << "\n";
      f << "总工作量 (W): " << gr.total_work << "\n";
      f << "核心数 (m): " << cores << "\n\n";
      f << "=== 基于优先级的调度 ===\n";
      f << "最坏响应时间: " << pr.worst_response_time << "\n";
      f << "=== Graham 界 ===\n";
      f << "Makespan 界: " << gr.graham_makespan_bound << "\n";
      f << "最坏响应时间: " << gr.worst_response_time << "\n";
      f << "=== 对比 ===\n";
      f << "紧化比: "
        << (pr.worst_response_time / gr.worst_response_time) << "\n";
      std::cout << "  已保存到 " << ResolveOutputPath(outfile) << "\n";
    } else {
      std::cout << "  无法打开文件。\n";
    }
  }

  PressEnter();
}

void RunParameterSweep() {
  std::cout << "\n--- 参数扫描 ---\n\n";
  std::cout << "0: 核心数\n1: 节点数\n";
  int param_choice = PromptInt("扫描参数（0=核心数, 1=节点数）", 0);
  std::string param_name = (param_choice == 1) ? "nodes" : "cores";

  int from = PromptInt("起始值", 1);
  int to = PromptInt("终止值", 8);
  int step = PromptInt("步长", 1);
  int fixed_cores = 4;
  std::string dag_name;

  if (param_name == "nodes") {
    fixed_cores = PromptInt("固定核心数", 4);
    dag_name = "random_varying";
  } else {
    std::cout << "DAG 格式: chain_N, fork_join_N, random_N, stg:<路径>\n";
    dag_name = PromptStr("DAG 名称", "random_20");
  }

  std::string outfile = PromptStr("输出文件（留空 = 终端）", "");

  std::ostream* out = &std::cout;
  std::ofstream fout;
  if (!outfile.empty()) {
    fout.open(ResolveOutputPath(outfile));
    if (!fout.is_open()) {
      std::cout << "  无法打开文件，输出到终端。\n";
    } else {
      out = &fout;
    }
  }
  std::ostream& os = *out;

  algorithms::NodePriorityAssignmentAlgorithm prio;
  algorithms::GrahamResponseTimeAlgorithm graham;
  algorithms::GeneratedDag base;
  if (param_name == "nodes") {
    base.label = dag_name;
  } else {
    base = algorithms::DAGGenerator::Resolve(dag_name);
    if (base.graph.NodeCount() == 0) {
      std::cout << "  加载 DAG 失败: " << dag_name << "\n";
      PressEnter();
      return;
    }
  }

  std::vector<int> values;
  for (int v = from; v <= to; v += step) values.push_back(v);

  std::cout << "\n正在扫描 " << values.size() << " 个点...\n";

  os << "{\n";
  utils::WriteString(os, "param_name", param_name);
  os << ",\n";
  os << "  \"param_values\": [";
  {
    utils::Comma comma;
    for (int v : values) {
      comma.Write(os);
      os << v;
    }
  }
  os << "],\n";
  utils::WriteString(os, "dag_label", base.label);
  os << ",\n";
  utils::WriteInt(os, "fixed_cores", (param_name == "nodes") ? fixed_cores : 0);
  os << ",\n";

  os << "  \"data_points\": [\n";
  utils::Comma point_comma;
  for (std::size_t pi = 0; pi < values.size(); ++pi) {
    int pv = values[pi];
    int cores = (param_name == "cores") ? pv : fixed_cores;

    dag::DagGraph g;
    if (param_name == "nodes") {
      auto gen = algorithms::DAGGenerator::RandomDAG(pv, 0.3, 42 + pi);
      g = gen.graph;
    } else {
      g = base.graph;
    }

    auto metrics = prio.ComputeCriticalPathMetrics(g);
    if (!metrics.valid) continue;
    auto prios_v = prio.AssignPrioritiesByCriticalPath(g, metrics);
    if (prios_v.empty()) continue;
    auto pr = prio.AnalyzeResponseTime(g, prios_v, cores);
    if (!pr.valid) continue;
    auto gr = graham.Analyze(g, cores);
    if (!gr.valid) continue;

    double tightening = 1.0;
    if (gr.worst_response_time > 0.0)
      tightening = pr.worst_response_time / gr.worst_response_time;

    point_comma.Write(os);
    os << "    {\n";
    utils::WriteInt(os, "param_value", pv);
    os << ",\n";
    utils::WriteDouble(os, "priority_worst_response_time",
                        pr.worst_response_time);
    os << ",\n";
    utils::WriteDouble(os, "graham_worst_response_time",
                        gr.worst_response_time);
    os << ",\n";
    utils::WriteDouble(os, "graham_makespan_bound",
                        gr.graham_makespan_bound);
    os << ",\n";
    utils::WriteDouble(os, "tightening_ratio", tightening);
    os << ",\n";
    utils::WriteInt64(os, "total_work", gr.total_work);
    os << ",\n";
    utils::WriteInt64(os, "critical_path_length",
                       gr.critical_path_length);
    os << "\n    }";
  }

  os << "\n  ]\n}\n";
  if (fout.is_open()) fout.close();
  std::cout << "完成。\n";
  PressEnter();
}

void RunMinimumCores() {
  std::cout << "\n--- 目标响应时间下求最小核心数 ---\n\n";
  std::cout << "DAG 格式: chain_N, fork_join_N, random_N, stg:<路径>\n";
  std::string dag_name = PromptStr("DAG 名称", "random_20");
  double target_from = PromptDouble("目标响应时间 起始", 0.0);
  double target_to = PromptDouble("目标响应时间 终止", 0.0);
  double target_step = PromptDouble("目标响应时间 步长", 0.0);
  int max_cores = PromptInt("最大核心数", 256);
  std::string outfile = PromptStr("输出文件（留空 = 终端）", "");

  algorithms::GeneratedDag gen =
      algorithms::DAGGenerator::Resolve(dag_name);
  if (gen.graph.NodeCount() == 0) {
    std::cout << "  加载 DAG 失败: " << dag_name << "\n";
    PressEnter();
    return;
  }

  // Auto-range
  if (target_from == 0.0 && target_to == 0.0) {
    algorithms::NodePriorityAssignmentAlgorithm prio;
    auto metrics = prio.ComputeCriticalPathMetrics(gen.graph);
    if (!metrics.valid) {
      std::cout << "  关键路径指标计算失败。\n";
      PressEnter();
      return;
    }
    target_from = static_cast<double>(metrics.critical_path_length);
    algorithms::GrahamResponseTimeAlgorithm graham;
    auto probe = graham.Analyze(gen.graph, 1);
    if (!probe.valid) {
      std::cout << "  响应时间探测失败。\n";
      PressEnter();
      return;
    }
    target_to = probe.worst_response_time;
    target_step = (target_to - target_from) / 10.0;
    if (target_step <= 0.0) target_step = 1.0;
    std::cout << "  自动范围: " << target_from << " ~ " << target_to
              << " 步长 " << target_step << "\n";
  }

  std::vector<double> targets;
  for (double v = target_from; v <= target_to + target_step * 0.5;
       v += target_step)
    targets.push_back(v);

  std::ostream* out = &std::cout;
  std::ofstream fout;
  if (!outfile.empty()) {
    fout.open(ResolveOutputPath(outfile));
    if (!fout.is_open()) {
      std::cout << "  无法打开文件，输出到终端。\n";
    } else {
      out = &fout;
    }
  }
  std::ostream& os = *out;

  algorithms::MinimumCoreAlgorithm mca;

  std::cout << "正在计算 " << targets.size() << " 个点...\n";

  os << "{\n";
  utils::WriteString(os, "dag_label", gen.label);
  os << ",\n";
  os << "  \"target_values\": [";
  {
    utils::Comma comma;
    for (double v : targets) {
      comma.Write(os);
      os << v;
    }
  }
  os << "],\n";

  os << "  \"data_points\": [\n";
  utils::Comma point_comma;
  for (double target : targets) {
    auto prio_result = mca.ComputeForPriority(gen.graph, target, max_cores);
    auto graham_result = mca.ComputeForGraham(gen.graph, target, max_cores);

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
  if (fout.is_open()) fout.close();
  std::cout << "完成。\n";
  PressEnter();
}

void ShowHelp() {
  std::cout << R"(
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
              使 用 说 明
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

[0] 单 DAG 分析 — 看一个具体例子的完整结果
  给定 DAG 和核心数，一次性算出：
  - 关键路径长度 (L) 与总工作量 (W)
  - 每个节点按关键路径分配的优先级顺序
  - 优先级调度下每个节点的最坏完成时间
  - Graham 界的 makespan 下界 L+(W-L)/m
  - 优先级调度相比 Graham 界的紧化比（越接近 1 说明 Graham
    界本身已经够紧，越小说明优先级调度改善越多）
  适用于：快速验证某个 DAG 在特定核心数下的表现。

[1] 参数扫描 — 画对比曲线的数据来源
  固定 DAG，让某个参数（核心数 m 或 DAG 节点数 n）在一定
  范围内逐步变化，每个点计算优先级和 Graham 两种调度方式
  的最坏响应时间。输出 JSON，之后用 Python 画折线图。
  举例：
  - 核心数 1→8：看加核之后响应时间怎么降
  - 节点数 20→100：看 DAG 变大之后紧化比怎么变
  适用于：生成论文中的对比图数据。

[2] 求目标响应时间下最小核心数 — 给定 deadline，求需要多少核
  给定一个目标响应时间（deadline），二分搜索出：
  - 优先级调度要达到这个 deadline 最少需要几个核
  - Graham 调度最少需要几个核
  - 两者核心数的比值
  可以给一个目标时间范围（如 50→120），让程序在范围内
  取点逐个计算。如果范围全填 0，会自动用 L ~ m=1 响应
  时间作为范围。
  适用于：回答"要满足 XX 毫秒的响应时间，至少得买多少核？"

DAG 命名格式:
  chain_N     — N 个节点的链状 DAG（纯顺序依赖）
  fork_join_N — N 个节点的 fork-join DAG（一对多再汇聚）
  random_N    — N 个节点的随机 DAG (edge_prob=0.3)
  stg:<路径>  — 从 STG 文件加载 DAG

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
)";
  PressEnter();
}

}  // namespace

int main() {
  while (!input_closed) {
    std::cout << "\n=== 多核 DAG 调度分析 ===\n\n";
    std::cout << "0: 单 DAG 分析 — 快速看一个具体例子的优先级调度与 Graham 界\n";
    std::cout << "1: 参数扫描 — 变化核心数或节点数，生成对比曲线数据 (JSON)\n";
    std::cout << "2: 求目标响应时间下最小核心数 — 给 deadline，算出需要多少核\n";
    std::cout << "3: 使用说明\n";
    std::cout << "4: 退出\n\n";

    int choice = PromptInt("请选择", 4);

    switch (choice) {
      case 0: RunSingleAnalysis(); break;
      case 1: RunParameterSweep(); break;
      case 2: RunMinimumCores(); break;
      case 3: ShowHelp(); break;
      case 4:
        std::cout << "再见。\n";
        return 0;
      default:
        std::cout << "  无效选项。\n";
        PressEnter();
    }
  }
}
