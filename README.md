# Multicore-DAG-Schedule

多核 DAG 调度算法的实验评估框架。核心工具是交互式终端程序 **`dag_terminal`**，支持三种分析模式，输出结构化 JSON，配合 Python 脚本生成论文级图表。

## 工作流

```
dag_terminal (交互式终端)  →  结构化 JSON  →  Python/Matplotlib  →  PNG/SVG/PDF 图表
```

1. 运行 `bazel run //:dag_terminal`，在菜单中选择分析模式并输入参数
2. 程序计算结果，输出 JSON（可重定向到文件）
3. 用 `scripts/` 下的 Python 脚本读取 JSON，生成图表

## 三种分析模式

### [0] 单 DAG 分析

给定一个 DAG 和核心数，计算优先级调度与 Graham 界的响应时间上界，直接在终端查看完整结果（关键路径、优先级顺序、每个节点的最坏完成时间、紧化比）。

### [1] 参数扫描

固定 DAG，让核心数（或节点数）在一定范围内变化，每个点计算两种调度方式的最坏响应时间。输出 JSON，供 `plot_comparison.py` 生成对比折线图。

```bash
# 终端内选择 [1]，按提示输入参数，结果保存到 sweep.json
bazel run //:dag_terminal

# 用 Python 画图
python3 scripts/plot_comparison.py -i sweep.json -o output/compare.png
```

### [2] 最小核心数搜索

给定目标响应时间范围，二分搜索出优先级调度和 Graham 调度各自需要的最小核心数。输出 JSON，供 `plot_cores_comparison.py` 生成核心数对比图。

```bash
# 终端内选择 [2]，按提示输入参数，结果保存到 cores.json
bazel run //:dag_terminal

# 用 Python 画图
python3 scripts/plot_cores_comparison.py -i cores.json -o output/cores.png
```

## DAG 数据来源

`dag_terminal` 支持两种 DAG 来源：

| 来源 | 格式 | 说明 |
|------|------|------|
| 内置生成器 | `chain_N` / `fork_join_N` / `random_N` | 程序内即时生成 |
| STG 文件 | `stg:<路径>` | 调度研究标准基准格式 |

STG 文件格式：
```
<任务总数>
<节点ID> <执行时间> <前驱数> [<前驱1> ...]
...
```

## Python 可视化脚本

所有脚本依赖 `matplotlib` 和 `numpy`：

```bash
pip3 install -r scripts/requirements.txt
```

| 脚本 | 输入 | 输出 |
|------|------|------|
| `plot_comparison.py` | 参数扫描 JSON（模式 [1]） | 优先级 vs Graham 响应时间对比图 |
| `plot_cores_comparison.py` | 最小核心数 JSON（模式 [2]） | 最小核心数对比图 + 核心数比值图 |
| `plot_terminal_architecture.py` | 无 | `dag_terminal` 系统架构图 |

## 构建与运行

```bash
# 构建全部
bazel build //...

# 运行交互式终端
bazel run //:dag_terminal

# 运行测试
bazel test //...
```

## 项目结构

```
.
├── BUILD.bazel
├── include/
│   ├── algorithms/          # 调度算法头文件
│   │   ├── node_priority_assignment_algorithm.h
│   │   ├── graham_response_time_analysis.h
│   │   ├── minimum_core_algorithm.h
│   │   └── dag_generator.h
│   ├── dag/                 # DAG 数据结构
│   │   ├── dag_graph.h
│   │   └── task_node.h
│   ├── io/
│   │   └── stg_reader.h     # STG 文件读取
│   └── utils/
│       └── json_utils.h     # JSON 输出辅助
├── src/
│   ├── terminal_app.cpp     # ★ dag_terminal 主程序
│   ├── algorithms/
│   ├── io/
│   └── dag_graph.cpp
├── scripts/                 # Python 可视化脚本
│   ├── plot_comparison.py
│   ├── plot_cores_comparison.py
│   ├── plot_terminal_architecture.py
│   └── requirements.txt
├── tests/
│   └── dag_graph_test.cpp
└── docs/
    ├── quickstart.md
    └── algorithms.md
```
