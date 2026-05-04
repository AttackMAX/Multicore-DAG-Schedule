# CLAUDE.md

本文件为 Claude Code（claude.ai/code）在此仓库中工作时提供指导。

## 构建系统

使用 Bazel 构建，模块名为 `multicore_dag_schedule`。

```bash
# 构建所有目标
bazel build //...

# 构建指定目标
bazel build //:dag_scheduler_app
bazel build //:dag_core
bazel build //:node_priority_assignment_algorithm

# 运行主程序
bazel run //:dag_scheduler_app

# 运行全部测试
bazel test //...

# 运行单个测试
bazel test //:dag_graph_test
```

## 架构

**命名空间与目录布局**：代码使用两个命名空间，与目录结构对应：
- `dag::` — DAG 数据结构（`include/dag/`、`src/dag_graph.cpp`）
- `algorithms::` — 调度算法（`include/algorithms/`、`src/algorithms/`）

**核心数据结构**（`include/dag/`）：

- `TaskNode` — DAG 中的节点，包含 `id`、`duration`（int64）和 `outgoing_edges`（后继节点 ID 的邻接表）。
- `DagGraph` — 邻接表 DAG。节点按 ID 插入；边引用节点 ID（而非索引）。关键方法：`AddNode(id, duration)`、`AddEdge(from, to)`、`TopologicalOrder()`（检测到环时返回空向量）、`GetNode(id)`、`Nodes()`。

**算法模块**（`include/algorithms/node_priority_assignment_algorithm.h`）：

`NodePriorityAssignmentAlgorithm` 是一个无状态类，包含三个流水线方法：

1. `ComputeCriticalPathMetrics(graph)` — 按拓扑序进行动态规划。计算 `lf`（前驱节点到当前节点的最长路径）、`lb`（当前节点到后继节点的最长路径）以及 `longest_path = lf + lb - duration`。失败时返回 `CriticalPathMetrics` 且 `valid = false`。

2. `AssignPrioritiesByCriticalPath(graph, metrics)` — 贪心优先级分配。从就绪集（入度为 0 的节点）中反复选择 `longest_path` 最大的节点（相同时取 ID 最小者）。返回节点 ID 的有序向量。

3. `AnalyzeResponseTime(graph, priorities, core_count)` — 计算干扰模型下每个节点的最坏完成时间。对于每个节点，干扰来自优先级高于它且非其祖先的节点，干扰量除以核心数。返回 `ResponseTimeAnalysis`，包含每个节点的完成时间及全局 `worst_response_time`。

**结果结构体**使用 `valid` 标志（默认 `false`）而非异常。三个算法方法在失败时返回空/不完整结果——调用方需检查 `valid` 字段。

**测试**（`tests/dag_graph_test.cpp`）是单个整体测试二进制文件（非测试框架），成功返回 0，失败返回非零。

## 代码规范

- C++ 使用 `#ifndef HEADER_H_ ... #define ...` 形式的头文件保护。
- 时长和路径长度使用 `std::int64_t`；响应时间使用 `double`。
- 节点 ID 查找为线性扫描（`NodeIndex` 遍历 `nodes_`）——每次调用 O(n)。
- `TopologicalOrder()` 检测到环时返回空向量。
- 新增算法遵循相同模式：头文件放在 `include/algorithms/`，实现放在 `src/algorithms/`，添加 Bazel `cc_library` 目标并依赖 `:dag_core`，同时添加 Bazel `cc_test` 目标。
