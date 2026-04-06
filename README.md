# Multicore-DAG-Schedule

## 项目简介

本项目是一个面向毕业设计的 DAG（有向无环图）调度算法实验框架，使用 C++ 实现、Bazel 组织构建。  
当前阶段重点完成了：

- DAG 数据结构（邻接表）与拓扑遍历基础能力
- 节点优先级分配算法的独立模块化实现
- 响应时间分析流程的可运行版本

项目目标是逐步沉淀多个算法，并在统一输入与指标下与本文着重的《基于节点优先级的多核处理器任务调度算法》进行实验评估。

## 当前目录结构

```tree
.
├── BUILD.bazel                                      # Bazel 构建目标定义
├── MODULE.bazel                                     # Bazel 模块配置
├── include/                                         # 头文件目录（对外接口/数据结构）
│   ├── algorithms/                                  # 算法模块接口声明
│   │   └── node_priority_assignment_algorithm.h
│   └── dag/                                         # DAG 图结构与节点定义
│       ├── dag_graph.h
│       └── task_node.h
├── src/                                             # 源码实现目录
│   ├── algorithms/                                  # 各算法实现文件
│   │   └── node_priority_assignment_algorithm.cpp
│   ├── dag_graph.cpp                                # DAG 基础能力实现（加点/加边/拓扑序）
│   └── main.cpp                                     # 示例入口（演示算法调用流程）
└── tests/                                           # 测试代码目录
    └── dag_graph_test.cpp
```

## 已实现算法

当前已实现一个独立算法模块：`NodePriorityAssignmentAlgorithm`，包含三部分：

1. 关键路径指标计算（动态规划）
   - 计算每个节点的 `lf`、`lb`、`l`
   - 输出全图关键路径长度

2. 节点优先级分配（贪心）
   - 在满足拓扑依赖的前提下，从 ready 集中选择 `l(vi)` 最大节点
   - 生成优先级序列

3. 响应时间分析
   - 基于优先级与干扰集合估算每个节点最坏完成时间
   - 输出全局 worst response time

## TODO

- 新增更多对比算法文件（每种算法一个 `include/algorithms/*.h + src/algorithms/*.cpp`）
- 抽象统一算法接口，便于批量对比运行
- 增加 DAG 输入方式（文件输入/随机生成）
- 补充实验评估脚本（多图规模、多核数、多算法）
- 完善测试覆盖（边界图、随机图、异常输入）
- 在本地安装 Bazel 后补齐 `bazel build/test` 全流程验证
