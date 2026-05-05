#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUTPUT_DIR="${PROJECT_ROOT}/output"
SCRIPTS_DIR="${PROJECT_ROOT}/scripts"

mkdir -p "${OUTPUT_DIR}"

# Check Python dependencies
echo "=== Checking Python dependencies ==="
python3 -c "import matplotlib; import numpy" 2>/dev/null || {
    echo "matplotlib and numpy are required. Install with:"
    echo "  pip install -r ${SCRIPTS_DIR}/requirements.txt"
    exit 1
}

echo "=== Building C++ binaries ==="
(cd "${PROJECT_ROOT}" && bazel build //:dag_scheduler_compare)

BIN="${PROJECT_ROOT}/bazel-bin/dag_scheduler_compare"

echo "=== Generating comparison curves ==="

# Core-count sweep on different DAG types
for dag in random_20 fork_join_30 chain_20; do
    echo "  Sweeping cores for ${dag}..."
    "${BIN}" --param cores --from 1 --to 16 --dag "${dag}" \
        | python3 "${SCRIPTS_DIR}/plot_comparison.py" \
            --output "${OUTPUT_DIR}/compare_cores_${dag}.png"
done

# DAG-size sweep with fixed core counts
for m in 2 4 8; do
    echo "  Sweeping nodes for m=${m}..."
    "${BIN}" --param nodes --from 5 --to 50 --step 5 --cores "${m}" \
        | python3 "${SCRIPTS_DIR}/plot_comparison.py" \
            --output "${OUTPUT_DIR}/compare_nodes_m${m}.png"
done

echo ""
echo "=== Done. Charts in ${OUTPUT_DIR}/ ==="
ls -la "${OUTPUT_DIR}/"
