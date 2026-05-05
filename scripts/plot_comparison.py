#!/usr/bin/env python3
"""
Read comparison JSON from stdin (or --input FILE) and generate a comparison
curve chart as PNG/SVG.

Usage:
    bazel run //:dag_scheduler_compare -- --param cores --from 1 --to 8 | \
        python3 scripts/plot_comparison.py --output output/compare.png

    python3 scripts/plot_comparison.py --input data.json --output chart.svg
"""

import argparse
import json
import sys

import matplotlib.pyplot as plt
import numpy as np


def main():
    parser = argparse.ArgumentParser(
        description="Plot worst-response-time upper bound comparison curves")
    parser.add_argument("--input", "-i",
                        help="Input JSON file (default: stdin)")
    parser.add_argument("--output", "-o", default="comparison.png",
                        help="Output chart file (default: comparison.png)")
    parser.add_argument("--format", "-f", choices=["png", "svg", "pdf"],
                        default=None,
                        help="Output format (default: inferred from extension)")
    args = parser.parse_args()

    if args.input:
        with open(args.input, "r") as f:
            data = json.load(f)
    else:
        data = json.load(sys.stdin)

    param_name = data["param_name"]
    param_label = ("Core Count (m)" if param_name == "cores"
                   else "DAG Size (nodes)")
    dag_label = data["dag_label"]
    param_values = np.array(data["param_values"])
    points = data["data_points"]

    prio_vals = np.array([p["priority_worst_response_time"] for p in points])
    graham_vals = np.array([p["graham_worst_response_time"] for p in points])
    graham_bound_vals = np.array([p["graham_makespan_bound"] for p in points])
    ratios = [p["tightening_ratio"] for p in points]

    fig, ax = plt.subplots(figsize=(10, 6))

    ax.plot(param_values, prio_vals,
            "o-", color="#2c7bb6", linewidth=2, markersize=6,
            label="Priority-Based Upper Bound")
    ax.plot(param_values, graham_vals,
            "s--", color="#d7191c", linewidth=2, markersize=6,
            label="Graham (Arbitrary) Upper Bound")
    ax.plot(param_values, graham_bound_vals,
            ":", color="#d7191c", linewidth=1.5, alpha=0.6,
            label="Graham Makespan Bound L+(W-L)/m")

    ax.fill_between(param_values, prio_vals, graham_vals,
                     alpha=0.15, color="#2c7bb6",
                     label="Tightening gap (priority gain)")

    # Annotate tightening ratios at sampled points
    annotate_step = max(1, len(param_values) // 5)
    for i in range(0, len(param_values), annotate_step):
        x = param_values[i]
        y_mid = (prio_vals[i] + graham_vals[i]) / 2
        ax.annotate(f"ratio={ratios[i]:.3f}",
                    xy=(x, y_mid),
                    fontsize=7, color="#555555",
                    ha="center", va="center",
                    bbox=dict(boxstyle="round,pad=0.2",
                              facecolor="white", alpha=0.8, edgecolor="none"))

    ax.set_xlabel(param_label, fontsize=12)
    ax.set_ylabel("Worst Response Time", fontsize=12)
    ax.set_title(f"Response-Time Upper Bound Comparison\n"
                 f"DAG: {dag_label}",
                 fontsize=13, fontweight="bold")
    ax.legend(loc="upper right", fontsize=9)
    ax.grid(True, alpha=0.3)

    # Add summary text box
    total_work = points[0].get("total_work", "?")
    cp_len = points[0].get("critical_path_length", "?")
    textstr = (f"Total work W = {total_work}\n"
               f"Critical path L = {cp_len}\n"
               f"Best tightening = {min(ratios):.3f}")
    props = dict(boxstyle="round,pad=0.5", facecolor="lightyellow",
                 alpha=0.9, edgecolor="gray")
    ax.text(0.98, 0.97, textstr, transform=ax.transAxes,
            fontsize=8, verticalalignment="top",
            horizontalalignment="right", bbox=props)

    fig.tight_layout()
    fmt = args.format or (args.output.rsplit(".", 1)[-1]
                           if "." in args.output else "png")
    fig.savefig(args.output, format=fmt, dpi=150)
    print(f"Chart saved to {args.output}")


if __name__ == "__main__":
    main()
