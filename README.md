# Multicore-DAG-Schedule

多核 DAG 调度算法的实验评估框架——面向毕业设计，验证**基于优先级的调度算法响应时间上界优于任意调度**。

## 核心问题

给定一个 DAG 任务图（节点带执行时间，边表依赖）和 `m` 个处理器核心，每种调度策略都能给出一个最坏响应时间上界。本项目通过实现并对比两种分析模型来回答：

> **优先级调度是否比任意调度提供更紧（更小）的响应时间上界？**

| 分析模型 | 干扰集定义 | 上界特征 |
|---------|-----------|---------|
| 优先级调度 | 仅限优先级更高且非祖先的节点 | 较紧（利用全序约束） |
| Graham 任意调度 | 所有不可比节点（非祖先非后代） | 较松（保守上界） |

两者的比值（紧致比）量化了优先级分配带来的收益。

## 文档导航

| 文档 | 内容 |
|------|------|
| [快速开始](./docs/quickstart.md) | 环境搭建、构建、运行 |
| [算法文档](./docs/algorithms.md) | 数据结构、两个算法的详细推导与示例 |

## 已实现算法

### 1. 优先级调度分析 (`NodePriorityAssignmentAlgorithm`)

三阶段流水线：关键路径指标计算 → 贪心优先级分配 → 干扰模型响应时间分析。

### 2. Graham 上界分析 (`GrahamResponseTimeAlgorithm`)

基于 Graham's Bound（`L + (W-L)/m`）和不可比节点干扰集，计算任意 work-conserving 调度下的最坏响应时间上界，作为对比基线。

## 目录结构

```
.
├── BUILD.bazel                           # Bazel 构建目标
├── MODULE.bazel                          # Bazel 模块依赖
├── gen_compile_commands.py               # clangd 编译数据库生成
├── docs/
│   ├── quickstart.md                     # 快速开始指南
│   └── algorithms.md                     # 算法详细文档
├── include/
│   ├── algorithms/
│   │   ├── node_priority_assignment_algorithm.h
│   │   └── graham_response_time_analysis.h
│   └── dag/
│       ├── dag_graph.h
│       └── task_node.h
├── src/
│   ├── algorithms/
│   │   ├── node_priority_assignment_algorithm.cpp
│   │   └── graham_response_time_analysis.cpp
│   ├── dag_graph.cpp
│   └── main.cpp                          # 双算法对比入口
└── tests/
    └── dag_graph_test.cpp
```

## 构建与运行

```bash
bazel build //...                         # 构建全部
bazel test //...                          # 运行测试
bazel run //:dag_scheduler_app            # 运行对比程序
```

详细环境搭建步骤见 [docs/quickstart.md](./docs/quickstart.md)。

## 下一步

- 新增更多对比算法（如 G-EDF 响应时间分析、Federated Scheduling）
- 支持外部 DAG 输入（文件读取 / 随机生成）
- 补充实验评估脚本（多图规模 × 多核数 × 多算法）
- 统一算法接口，便于批量对比
- 可视化调度甘特图与上界对比曲线
