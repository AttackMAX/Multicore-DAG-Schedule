# 算法文档

## 目录

- [数据结构：`dag::TaskNode`](#数据结构dagtasknode)
- [数据结构：`dag::DagGraph`](#数据结构dagdaggraph)
- [算法模块：`algorithms::NodePriorityAssignmentAlgorithm`](#算法模块algorithmsnodepriorityassignmentalgorithm)
  - [1. `ComputeCriticalPathMetrics` — 关键路径指标计算](#1-computecriticalpathmetrics--关键路径指标计算)
  - [2. `AssignPrioritiesByCriticalPath` — 节点优先级分配](#2-assignprioritiesbycriticalpath--节点优先级分配)
  - [3. `AnalyzeResponseTime` — 响应时间分析](#3-analyzeresponsetime--响应时间分析)
- [算法模块：`algorithms::GrahamResponseTimeAlgorithm`](#算法模块algorithmsgrahamresponsetimealgorithm)
  - [`Analyze` — Graham 任意调度响应时间分析](#analyze--graham-任意调度响应时间分析)

---

## 数据结构：`dag::TaskNode`

**文件：** `include/dag/task_node.h`

表示 DAG 中的一个任务节点。

### 字段

| 字段 | 类型 | 含义 |
|------|------|------|
| `id` | `int` | 节点唯一标识，用户定义 |
| `duration` | `std::int64_t` | 任务执行时间（处理器周期） |
| `outgoing_edges` | `std::vector<int>` | 后继节点的 ID 列表（邻接表） |

### 示例

```cpp
dag::TaskNode node;
node.id = 1;
node.duration = 5;
node.outgoing_edges = {3, 4};  // 节点 1 指向节点 3 和节点 4
```

---

## 数据结构：`dag::DagGraph`

**文件：** `include/dag/dag_graph.h` / `src/dag_graph.cpp`

邻接表 DAG。节点通过用户定义的 `id` 引用，边通过 `AddEdge(from, to)` 添加。

### 方法

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `AddNode(id, duration)` | `bool` | 添加节点。节点 ID 已存在时返回 `false` |
| `AddEdge(from, to)` | `bool` | 添加有向边 from→to。任一端点不存在时返回 `false` |
| `ContainsNode(id)` | `bool` | 检查节点是否存在 |
| `NodeCount()` | `std::size_t` | 返回节点总数 |
| `Nodes()` | `const std::vector<TaskNode>&` | 返回所有节点的引用 |
| `GetNode(id)` | `const TaskNode*` | 按 ID 查找节点，不存在返回 `nullptr` |
| `TopologicalOrder()` | `std::vector<int>` | 返回拓扑排序的节点 ID 列表。检测到环时返回**空向量** |

### `TopologicalOrder()` 详述

使用 Kahn 算法（BFS）：
1. 统计所有节点的入度
2. 将入度为 0 的节点加入就绪队列
3. 每次从队列弹出节点加入结果，将其后继节点入度减 1
4. 若入度减至 0 则加入队列
5. 若结果长度 ≠ 节点总数，说明存在环，返回空向量

**时间复杂度：** O(V + E)

**输入示例：**

```
节点 1 (duration=5)
节点 2 (duration=3)
节点 3 (duration=8)
节点 4 (duration=2)

边: 1→3, 2→3, 3→4
```

**输出：**
```
[1, 2, 3, 4]   （可能的拓扑序之一）
```

---

## 算法模块：`algorithms::NodePriorityAssignmentAlgorithm`

**文件：** `include/algorithms/node_priority_assignment_algorithm.h` / `src/algorithms/node_priority_assignment_algorithm.cpp`

无状态类，包含三个流水线方法，依次调用完成完整的优先级调度分析。

---

### 1. `ComputeCriticalPathMetrics` — 关键路径指标计算

```cpp
CriticalPathMetrics ComputeCriticalPathMetrics(const dag::DagGraph& graph) const;
```

**功能：** 通过动态规划，按拓扑正序/逆序两次遍历，计算每个节点的三个指标及全图关键路径长度。

**输入：**
| 参数 | 类型 | 含义 |
|------|------|------|
| `graph` | `const dag::DagGraph&` | DAG 图 |

**输出：** `CriticalPathMetrics`

| 字段 | 类型 | 含义 |
|------|------|------|
| `lf[id]` | `std::int64_t` | 前驱到当前节点的最长路径（含自身 duration） |
| `lb[id]` | `std::int64_t` | 当前节点到后继的最长路径（含自身 duration） |
| `longest_path[id]` | `std::int64_t` | 经过当前节点的最长路径长度 = `lf + lb - duration` |
| `critical_path_length` | `std::int64_t` | 全图关键路径长度 = `max(longest_path)` |
| `valid` | `bool` | `true` 表示计算成功 |

**算法流程：**

1. **构建前驱表** — 遍历所有节点的出边，为每个节点收集前驱 ID 列表
2. **拓扑正序计算 `lf`** — `lf[v] = duration[v] + max(lf[前驱])`
3. **拓扑逆序计算 `lb`** — `lb[v] = duration[v] + max(lb[后继])`
4. **计算 `longest_path`** — `l[v] = lf[v] + lb[v] - duration[v]`，取最大值作为 `critical_path_length`

**输入示例：**

```
节点: 1(dur=5), 2(dur=3), 3(dur=8), 4(dur=2)
边:   1→3, 2→3, 3→4
```

```
    1 (5)
     ↘
2 (3) → 3 (8) → 4 (2)
```

**输出：**

| 节点 | lf | lb | longest_path (l) |
|------|----|----|--------------------|
| 1 | 5 | 15 | 15 |
| 2 | 3 | 13 | 11 |
| 3 | 13 | 10 | 15 |
| 4 | 15 | 2 | 15 |

`critical_path_length = 15`

**失败情况：** 图中存在环时 `valid = false`，所有字段保持默认值。

---

### 2. `AssignPrioritiesByCriticalPath` — 节点优先级分配

```cpp
std::vector<int> AssignPrioritiesByCriticalPath(
    const dag::DagGraph& graph, const CriticalPathMetrics& metrics) const;
```

**功能：** 基于关键路径的贪心优先级分配。在满足拓扑依赖的前提下，每次从就绪集中选择 `longest_path` 最大的节点赋予当前最高优先级。

**输入：**
| 参数 | 类型 | 含义 |
|------|------|------|
| `graph` | `const dag::DagGraph&` | DAG 图 |
| `metrics` | `const CriticalPathMetrics&` | 步骤 1 计算出的关键路径指标 |

**输出：** `std::vector<int>` — 按优先级从高到低排列的节点 ID 列表。失败时返回空向量。

**算法流程：**

1. 计算所有节点入度
2. 将入度为 0 的节点加入 `ready_set`
3. 循环直到 `ready_set` 为空：
   - 从 `ready_set` 中选择 `longest_path` 最大的节点（相同时取 ID 最小者）
   - 将该节点加入优先级序列
   - 将该节点的后继入度减 1，入度归零的后继加入 `ready_set`

**贪心依据：** 关键路径越长的节点对整体调度影响越大，应优先调度。

**输入示例（沿用同一 DAG）：**

```
节点: 1(dur=5), 2(dur=3), 3(dur=8), 4(dur=2)
边:   1→3, 2→3, 3→4
metrics.longest_path: {1:15, 2:11, 3:15, 4:15}
```

**执行过程：**

| 轮次 | ready_set | 选择 | 原因 |
|------|-----------|------|------|
| 1 | {1, 2} | 1 | l(1)=15 > l(2)=11 |
| 2 | {2} | 2 | 唯一就绪 |
| 3 | {3} | 3 | 唯一就绪 |
| 4 | {4} | 4 | 唯一就绪 |

**输出：**
```
[1, 2, 3, 4]
```

**失败情况：** `metrics.valid == false` 或图中存在环时返回空向量。

---

### 3. `AnalyzeResponseTime` — 响应时间分析

```cpp
ResponseTimeAnalysis AnalyzeResponseTime(
    const dag::DagGraph& graph,
    const std::vector<int>& priorities,
    int core_count) const;
```

**功能：** 在给定优先级和核心数下，计算干扰模型下每个节点的最坏完成时间（finish time）。

**输入：**
| 参数 | 类型 | 含义 |
|------|------|------|
| `graph` | `const dag::DagGraph&` | DAG 图 |
| `priorities` | `const std::vector<int>&` | 步骤 2 输出的优先级序列 |
| `core_count` | `int` | 处理器核心数 |

**输出：** `ResponseTimeAnalysis`

| 字段 | 类型 | 含义 |
|------|------|------|
| `finish_time[id]` | `double` | 节点 `id` 的最坏完成时间 |
| `worst_response_time` | `double` | 全局最坏响应时间 = `max(finish_time)` |
| `valid` | `bool` | `true` 表示分析成功 |

**干扰模型：**

对每个节点 `v`，计算其最坏完成时间：

```
finish_time[v] = max(finish_time[前驱])             // 前驱依赖
               + duration[v]                         // 自身执行
               + Σ(干扰任务 duration) / core_count    // 干扰
```

其中**干扰任务**需同时满足三个条件：
1. 优先级**高于**当前节点（排在优先级序列更靠前）
2. **不是**当前节点的祖先（祖先在拓扑上必然先执行，不构成干扰）
3. 不是当前节点自身

干扰量除以核心数体现了多核并行对干扰的稀释效应。

**输入示例（沿用同一 DAG，core_count=2）：**

```
节点: 1(dur=5), 2(dur=3), 3(dur=8), 4(dur=2)
边:   1→3, 2→3, 3→4
priorities: [1, 2, 3, 4]
core_count = 2
```

**执行过程：**

**节点 1（拓扑序第一个）：**
- 无前驱 → pred_finish_max = 0
- 祖先：{}
- 优先级高于 1 的节点：无
- 干扰：0
- `finish_time[1] = 0 + 5 + 0 = 5.0`

**节点 2（拓扑序第一个，并行）：**
- 无前驱 → pred_finish_max = 0
- 祖先：{}
- 优先级高于 2 的节点：{1}，但 1 非祖先，1 也不在 2 的祖先集中
- 干扰：`duration[1] = 5`
- `finish_time[2] = 0 + 3 + 5/2 = 5.5`

**节点 3：**
- 前驱完成 max: `max(finish[1], finish[2]) = max(5.0, 5.5) = 5.5`
- 祖先：{1, 2}
- 优先级高于 3 的节点：{1, 2}，但两者都是祖先，被排除
- 干扰：0
- `finish_time[3] = 5.5 + 8 + 0 = 13.5`

**节点 4：**
- 前驱完成 max: `finish[3] = 13.5`
- 祖先：{1, 2, 3}
- 优先级高于 4 的节点：{1, 2, 3}，全部为祖先，被排除
- 干扰：0
- `finish_time[4] = 13.5 + 2 + 0 = 15.5`

**输出：**

| 节点 | finish_time |
|------|-------------|
| 1 | 5.0 |
| 2 | 5.5 |
| 3 | 13.5 |
| 4 | 15.5 |

`worst_response_time = 15.5`

**失败情况：** `core_count <= 0`、优先级序列长度与节点数不匹配、图中存在环时，`valid = false`。

---

## 算法模块：`algorithms::GrahamResponseTimeAlgorithm`

**文件：** `include/algorithms/graham_response_time_analysis.h` / `src/algorithms/graham_response_time_analysis.cpp`

无状态类，提供与优先级调度算法对应的任意调度最坏情况分析，用作对比基线。

---

### `Analyze` — Graham 任意调度响应时间分析

```cpp
GrahamResponseTimeAnalysis Analyze(const dag::DagGraph& graph,
                                    int core_count) const;
```

**功能：** 计算任意 work-conserving 调度下的最坏响应时间上界。不依赖优先级，仅基于 DAG 结构（祖先/后代关系）推导干扰集。

**输入：**
| 参数 | 类型 | 含义 |
|------|------|------|
| `graph` | `const dag::DagGraph&` | DAG 图 |
| `core_count` | `int` | 处理器核心数 |

**输出：** `GrahamResponseTimeAnalysis`

| 字段 | 类型 | 含义 |
|------|------|------|
| `finish_time[id]` | `double` | 节点 `id` 的最坏完成时间 |
| `worst_response_time` | `double` | 全局最坏响应时间 = `max(finish_time)` |
| `graham_makespan_bound` | `double` | Graham 全局上界 = `L + (W - L) / m` |
| `critical_path_length` | `std::int64_t` | 关键路径长度 L |
| `total_work` | `std::int64_t` | 总工作量 W = Σ duration |
| `valid` | `bool` | `true` 表示分析成功 |

**Graham 上界公式（Graham 1969）：**

```
makespan ≤ L + (W - L) / m
```

其中 `L` 为关键路径长度，`W` 为总工作量，`m` 为核心数。直观含义：关键路径节点必须串行执行（花费 L），剩余工作量 `W - L` 最多分散到 `m` 个核心并行，构成关键路径被延迟的上限。

**干扰模型：**

与优先级调度算法使用相同的递推框架，但干扰集定义不同：

```
finish_time[v] = max(finish_time[前驱])
               + duration[v]
               + Σ(干扰任务 duration) / core_count
```

其中**干扰任务**的定义为：与当前节点**不可比的节点**（既不是祖先也不是后代）。即：

```
干扰集(v) = { u | u 不是 v 的祖先, u 不是 v 的后代, u ≠ v }
```

与优先级方法的对比：

| 维度 | 优先级调度 | Graham（任意调度） |
|------|-----------|-------------------|
| 干扰集 | 更高优先级的不可比节点 | **所有**不可比节点 |
| 排除条件 | 优先级全序 + 祖先集 | 祖先集 + 后代集 |
| 上界松紧 | 较紧 | 更悲观（保守） |

优先级全序约束能缩小干扰集，因此其响应时间上界必然 ≤ Graham 上界。两者的比值体现了优先级分配带来的紧致性提升。

**输入示例（沿用同一 DAG，core_count=2）：**

```
节点: 1(dur=5), 2(dur=3), 3(dur=8), 4(dur=2)
边:   1→3, 2→3, 3→4
core_count = 2
L = 15, W = 18
```

**执行过程：**

**节点 1：**
- 祖先：{}, 后代：{3, 4}
- 不可比节点：{2}（既不是 1 的祖先也不是后代）
- 干扰：[2] duration = 3
- `finish_time[1] = 0 + 5 + 3/2 = 6.5`

**节点 2：**
- 祖先：{}, 后代：{3, 4}
- 不可比节点：{1}
- 干扰：[1] duration = 5
- `finish_time[2] = 0 + 3 + 5/2 = 5.5`

**节点 3：**
- 祖先：{1, 2}, 后代：{4}
- 不可比节点：{}（所有 4 个节点均被祖先/后代包含）
- 干扰：0
- `finish_time[3] = max(6.5, 5.5) + 8 + 0 = 14.5`

**节点 4：**
- 祖先：{1, 2, 3}, 后代：{}
- 不可比节点：{}
- 干扰：0
- `finish_time[4] = 14.5 + 2 + 0 = 16.5`

**输出：**

| 节点 | finish_time (优先级) | finish_time (Graham) |
|------|---------------------|---------------------|
| 1 | 5.0 | 6.5 |
| 2 | 5.5 | 5.5 |
| 3 | 13.5 | 14.5 |
| 4 | 15.5 | 16.5 |

```
worst_response_time (优先级) = 15.5
worst_response_time (Graham)  = 16.5
Graham makespan bound L+(W-L)/m = 16.5

紧致比 = 15.5 / 16.5 ≈ 0.939
```

**失败情况：** `core_count <= 0` 或图中存在环时，`valid = false`。

---

## 完整流水线示例

`src/main.cpp` 中的端到端调用，同时运行两种算法并对比：

```cpp
// 构建 DAG
dag::DagGraph graph;
graph.AddNode(1, 5);
graph.AddNode(2, 3);
graph.AddNode(3, 8);
graph.AddNode(4, 2);
graph.AddEdge(1, 3);
graph.AddEdge(2, 3);
graph.AddEdge(3, 4);

const int core_count = 2;

// === 优先级调度分析 ===
algorithms::NodePriorityAssignmentAlgorithm priority_algo;

auto metrics = priority_algo.ComputeCriticalPathMetrics(graph);
// metrics.critical_path_length = 15

auto priorities = priority_algo.AssignPrioritiesByCriticalPath(graph, metrics);
// priorities = [1, 2, 3, 4]

auto prio_response = priority_algo.AnalyzeResponseTime(graph, priorities, core_count);
// prio_response.worst_response_time = 15.5

// === Graham 任意调度分析 ===
algorithms::GrahamResponseTimeAlgorithm graham_algo;

auto graham_response = graham_algo.Analyze(graph, core_count);
// graham_response.worst_response_time = 16.5
// graham_response.graham_makespan_bound = 16.5

// === 对比 ===
// prio_response.worst_response_time <= graham_response.worst_response_time ✓
// 紧致比 ≈ 0.939
```
