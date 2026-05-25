#!/usr/bin/env python3
"""
Read cores-comparison JSON from stdin (or --input FILE) and generate a
minimum-cores comparison chart.

Usage:
    bazel run //:dag_cores_compare -- --dag random_20 | \
        python3 scripts/plot_cores_comparison.py --output output/cores_compare.png

    python3 scripts/plot_cores_comparison.py --input data.json --output chart.svg
"""

import argparse
import json
import sys

import matplotlib.pyplot as plt
import numpy as np


def main():
    parser = argparse.ArgumentParser(
        description="Plot minimum-cores comparison chart")
    parser.add_argument("--input", "-i",
                        help="Input JSON file (default: stdin)")
    parser.add_argument("--output", "-o", default="cores_comparison.png",
                        help="Output chart file (default: cores_comparison.png)")
    parser.add_argument("--format", "-f", choices=["png", "svg", "pdf"],
                        default=None,
                        help="Output format (default: inferred from extension)")
    args = parser.parse_args()

    if args.input:
        with open(args.input, "r") as f:
            data = json.load(f)
    else:
        data = json.load(sys.stdin)

    dag_label = data["dag_label"]
    target_values = np.array(data["target_values"])
    points = data["data_points"]

    prio_cores = []
    graham_cores = []
    prio_actual = []
    graham_actual = []
    ratios = []
    valid_targets = []
    valid_idx = []

    for i, p in enumerate(points):
        pc = p.get("priority_min_cores")
        gc = p.get("graham_min_cores")
        if pc is not None and gc is not None:
            valid_idx.append(i)
            valid_targets.append(target_values[i])
            prio_cores.append(pc)
            graham_cores.append(gc)
            prio_actual.append(p.get("priority_actual_response_time"))
            graham_actual.append(p.get("graham_actual_response_time"))
            r = p.get("core_ratio")
            ratios.append(r if r is not None else float("nan"))

    if not valid_targets:
        print("No feasible data points to plot.")
        return 1

    prio_cores = np.array(prio_cores)
    graham_cores = np.array(graham_cores)
    valid_targets = np.array(valid_targets)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(13, 11),
                                    gridspec_kw={"height_ratios": [2.5, 1]})

    # --- Top chart: min cores vs target ---
    ax1.plot(valid_targets, prio_cores,
             "o-", color="#2c7bb6", linewidth=2.5, markersize=9,
             label="Priority-Based Min Cores")
    ax1.plot(valid_targets, graham_cores,
             "s--", color="#d7191c", linewidth=2.5, markersize=9,
             label="Graham Min Cores")

    ax1.fill_between(valid_targets, prio_cores, graham_cores,
                     alpha=0.15, color="#2c7bb6",
                     label="Core savings (priority advantage)")

    # Annotate core counts
    for i in range(0, len(valid_targets)):
        if i % 3 != 0 and i != len(valid_targets) - 1:
            continue
        x = valid_targets[i]
        ax1.annotate(f"{prio_cores[i]}",
                     xy=(x, prio_cores[i]),
                     fontsize=9, color="#2c7bb6",
                     ha="center", va="bottom",
                     xytext=(0, 5), textcoords="offset points")
        ax1.annotate(f"{graham_cores[i]}",
                     xy=(x, graham_cores[i]),
                     fontsize=9, color="#d7191c",
                     ha="center", va="bottom",
                     xytext=(0, 5), textcoords="offset points")

    ax1.set_xlabel("Target Worst Response Time", fontsize=15)
    ax1.set_ylabel("Minimum Cores Required", fontsize=15)
    ax1.set_title(f"Minimum Cores to Meet Response-Time Target\n"
                  f"DAG: {dag_label}",
                  fontsize=17, fontweight="bold")
    ax1.legend(loc="upper right", fontsize=12)
    ax1.tick_params(axis="both", labelsize=12)
    ax1.grid(True, alpha=0.3)

    # --- Bottom chart: core ratio ---
    ax2.plot(valid_targets, ratios,
             "D-", color="#5e3c99", linewidth=2.5, markersize=8)
    ax2.axhline(y=1.0, color="gray", linestyle=":", linewidth=1, alpha=0.7)
    ax2.fill_between(valid_targets, ratios, 1.0,
                     where=(np.array(ratios) < 1.0),
                     alpha=0.15, color="#5e3c99",
                     label="Priority / Graham ratio")
    ax2.set_xlabel("Target Worst Response Time", fontsize=15)
    ax2.set_ylabel("Core Ratio", fontsize=15)
    ax2.set_title("Core Ratio (Priority / Graham)", fontsize=15)
    ax2.grid(True, alpha=0.3)
    ax2.tick_params(axis="both", labelsize=12)
    ax2.legend(loc="upper right", fontsize=12)

    # Annotate ratio values
    for i in range(0, len(valid_targets)):
        if i % 3 != 0 and i != len(valid_targets) - 1:
            continue
        if not np.isnan(ratios[i]):
            ax2.annotate(f"{ratios[i]:.2f}",
                         xy=(valid_targets[i], ratios[i]),
                         fontsize=9, color="#5e3c99",
                         ha="center", va="bottom",
                         xytext=(0, 5), textcoords="offset points")

    # Summary text
    best_ratio = min(r for r in ratios if not np.isnan(r))
    worst_ratio = max(r for r in ratios if not np.isnan(r))
    avg_saving = np.mean([(1 - r) * 100 for r in ratios if not np.isnan(r)])

    textstr = (f"Best ratio = {best_ratio:.3f}  (max core savings)\n"
               f"Worst ratio = {worst_ratio:.3f}\n"
               f"Avg core savings = {avg_saving:.1f}%")
    props = dict(boxstyle="round,pad=0.5", facecolor="lightyellow",
                 alpha=0.9, edgecolor="gray")
    ax1.text(0.98, 0.97, textstr, transform=ax1.transAxes,
             fontsize=11, verticalalignment="top",
             horizontalalignment="right", bbox=props)

    fig.tight_layout()
    fmt = args.format or (args.output.rsplit(".", 1)[-1]
                           if "." in args.output else "png")
    fig.savefig(args.output, format=fmt, dpi=200)
    print(f"Chart saved to {args.output}")


if __name__ == "__main__":
    main()
