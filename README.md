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

## 外部 DAG 输入

支持从文件加载 DAG，目前支持 **Standard Task Graph Set (STG)** 格式——调度研究中广泛使用的基准测试集。

### STG 格式

STG 文件使用固定宽度空格分隔字段：

```
<任务总数>
<节点ID> <执行时间> <前驱数> [<前驱1> ...]
...
```

文件中包含两个虚拟节点（源节点 ID=0 和汇节点 ID=N+1），加载时自动剥离。

### 使用方式

所有程序的 `--dag` 参数均支持 `stg:<路径>` 前缀：

```bash
# 单次分析
bazel run //:dag_scheduler_app -- --dag stg:$PWD/stg/50/rand0001.stg

# 核心数扫描
bazel run //:dag_scheduler_compare -- --dag stg:stg/100/rand0001.stg \
    --param cores --from 2 --to 8

# 最小组核心数分析
bazel run //:dag_cores_compare -- --dag stg:stg/500/rand0001.stg --max-cores 32
```

同时也支持内置生成器命名的 DAG：`chain_N`、`fork_join_N`、`random_N`。

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
│   │   ├── graham_response_time_analysis.h
│   │   ├── dag_generator.h
│   │   └── minimum_core_algorithm.h
│   ├── dag/
│   │   ├── dag_graph.h
│   │   └── task_node.h
│   ├── io/
│   │   └── stg_reader.h                  # STG 文件读取器
│   └── utils/
│       └── json_utils.h
├── src/
│   ├── algorithms/
│   │   ├── node_priority_assignment_algorithm.cpp
│   │   ├── graham_response_time_analysis.cpp
│   │   ├── dag_generator.cpp
│   │   └── minimum_core_algorithm.cpp
│   ├── io/
│   │   └── stg_reader.cpp               # STG 格式解析实现
│   ├── dag_graph.cpp
│   ├── main.cpp                         # 基本对比程序（支持 --dag）
│   ├── compare_main.cpp                 # 参数扫描工具
│   └── compare_cores_main.cpp           # 最小核心数分析
├── scripts/
│   └── run_pipeline.sh                  # 批量对比脚本
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

## 可视化

支持参数扫描对比图，直观展示两种算法的上界差异：

```bash
pip3 install --user matplotlib numpy
bash scripts/run_pipeline.sh       # 批量生成 output/*.png
```

图表类型：
- **核心数扫描**：固定 DAG，横轴 m=1..N，纵轴 worst response time
- **DAG 规模扫描**：固定核心数，横轴节点数，纵轴 worst response time

详见 [docs/quickstart.md](./docs/quickstart.md#6-生成可视化对比图)。

## 下一步

- 新增更多对比算法（如 G-EDF 响应时间分析、Federated Scheduling）
- 支持更多外部 DAG 输入格式（如 DAGgen、TGFF）
- 统一算法接口，便于批量对比
- STG 基准数据批量评测脚本
