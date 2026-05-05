# 快速开始

从零搭建环境到成功运行项目的完整流程。

## 1. 拉取代码

```bash
git clone <repo-url>
cd Multicore-DAG-Schedule
```

## 2. 安装依赖

项目需要 C++ 工具链、Bazel 构建系统、以及 clangd（可选，用于 IDE 智能提示）。

**安装 clang 工具链：**

```bash
sudo apt update
sudo apt install -y clang-18 clangd-18 lld-18 libc++-18-dev g++ make

# 设置系统默认指向 clang-18
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100
sudo update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-18 100
sudo update-alternatives --install /usr/bin/lld lld /usr/bin/lld-18 100
```

验证：

```bash
clang --version
clangd --version
```

**安装 Bazel（推荐使用 Bazelisk）：**

```bash
wget https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-amd64
chmod +x bazelisk-linux-amd64
sudo mv bazelisk-linux-amd64 /usr/local/bin/bazel
```

> 首次运行 `bazel` 时 Bazelisk 会下载 Bazel 本体，可能需要几分钟。
> 如果下载过慢，可设置镜像：
> ```bash
> export BAZELISK_BASE_URL=https://mirror.ghproxy.com/https://github.com/bazelbuild/bazel/releases/download
> ```

验证：

```bash
bazel --version
```

## 3. 构建项目

```bash
bazel build //...
```

## 4. 运行测试

```bash
# 运行全部测试
bazel test //...

# 运行测试并查看详细输出
bazel test //:dag_graph_test --test_output=all
```

## 5. 运行主程序

```bash
bazel run //:dag_scheduler_app
```

## 6. 生成可视化对比图

### 安装 Python 依赖

```bash
pip3 install --user matplotlib numpy
```

### 生成对比图

```bash
# 单张图：核心数扫描
bazel run //:dag_scheduler_compare -- --param cores --from 1 --to 8 --dag random_20 \
    | python3 scripts/plot_comparison.py --output output/compare.png

# 批量生成（6 张图，覆盖不同 DAG 类型和参数维度）
bash scripts/run_pipeline.sh
```

生成的 PNG 图片在 `output/` 目录中。

## 7. （可选）配置 VS Code clangd 智能提示

生成编译数据库：

```bash
python3 gen_compile_commands.py
```

在 VS Code 中安装扩展 `llvm-vs-code-extensions.vscode-clangd`，项目已包含 `.vscode/settings.json`，开箱即用。

> 每次新增 `.cpp` 文件后，重新运行 `python3 gen_compile_commands.py` 即可更新索引。
