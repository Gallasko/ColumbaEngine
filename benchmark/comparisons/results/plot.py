#!/usr/bin/env python3
"""
Plot cross-engine ECS benchmark results.

Reads a CSV emitted by the *_ecs_bench executables (schema in scenarios.h),
aggregates min/median per (engine, scenario, count), and produces two charts:

  summary/baseline_1M.png    bar chart at 1M entities — the headline snapshot
  summary/scaling.png        per-scenario ns/entity vs entity count

Usage:
    python3 plot.py [path/to/results.csv]

Defaults to ../results/raw/four_way.csv. Output goes to ../results/summary/.

PgEngine is highlighted: bold red bar/line, hatched bars, thicker line.
"""

import csv
import os
import sys
from collections import defaultdict
from datetime import datetime
from statistics import median

import matplotlib.pyplot as plt
import numpy as np


# ---- defaults ----
SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CSV = os.path.join(SCRIPT_DIR, "raw", "four_way.csv")
OUTPUT_DIR  = os.path.join(SCRIPT_DIR, "summary")

# Canonical scenario order — match scenarios.h.
SCENARIOS = [
    "create_empty",
    "create_bulk",
    "create_1comp",
    "create_2comp",
    "iterate_read_1comp",
    "iterate_write_1comp",
    "iterate_apply_velocity",
]

# Engine display order + colors. PgEngine first so it draws on top of others
# in line charts; red so it stands out as the engine-under-investigation.
ENGINE_STYLE = {
    "pgengine": {"color": "#d62728", "label": "PgEngine", "lw": 2.6, "hatch": "//"},
    "entt":     {"color": "#1f77b4", "label": "EnTT",     "lw": 1.6, "hatch": ""},
    "flecs":    {"color": "#2ca02c", "label": "flecs",    "lw": 1.6, "hatch": ""},
    "bevy":     {"color": "#9467bd", "label": "Bevy",     "lw": 1.6, "hatch": ""},
}
ENGINE_ORDER = ["pgengine", "entt", "flecs", "bevy"]


def load(csv_path):
    """Returns {(engine, scenario, count): [per_entity_ns, ...]}."""
    runs = defaultdict(list)
    with open(csv_path) as f:
        for r in csv.DictReader(f):
            runs[(r["engine"], r["scenario"], int(r["entity_count"]))].append(
                float(r["per_entity_ns"])
            )
    return runs


def mins(runs):
    return {k: min(v) for k, v in runs.items()}


def medians(runs):
    return {k: median(v) for k, v in runs.items()}


def plot_summary_1M(stats_min, stats_med, engines, scenarios, out_path):
    """One bar chart, all scenarios at 1M entities. The 'baseline snapshot'."""
    N = 1_000_000
    x = np.arange(len(scenarios))
    width = 0.8 / len(engines)

    fig, ax = plt.subplots(figsize=(13, 6.5))

    for i, e in enumerate(engines):
        ys = [stats_min.get((e, s, N), 0) for s in scenarios]
        offset = (i - len(engines) / 2 + 0.5) * width
        style = ENGINE_STYLE[e]
        bars = ax.bar(
            x + offset, ys, width,
            label=style["label"],
            color=style["color"],
            hatch=style["hatch"] if e == "pgengine" else "",
            edgecolor="black",
            linewidth=1.1 if e == "pgengine" else 0.5,
        )
        # Value labels above each bar.
        for bar, y in zip(bars, ys):
            if y <= 0:
                continue
            ax.text(
                bar.get_x() + bar.get_width() / 2, y,
                f"{y:.1f}",
                ha="center", va="bottom",
                fontsize=8,
                rotation=0,
            )

    ax.set_yscale("log")
    ax.set_xticks(x)
    ax.set_xticklabels(scenarios, rotation=25, ha="right")
    ax.set_ylabel("ns / entity  (min of 5 runs, lower is better)")
    ax.set_title(
        f"ECS Benchmark Baseline — {N:,} entities\n"
        f"PgEngine vs EnTT vs flecs vs Bevy   |   captured {datetime.now():%Y-%m-%d}"
    )
    ax.grid(True, axis="y", which="both", alpha=0.3)
    ax.legend(loc="upper right", framealpha=0.95)

    plt.tight_layout()
    plt.savefig(out_path, dpi=130)
    plt.close()


def plot_scaling(stats_min, engines, scenarios, counts, out_path):
    """4×2 grid (one subplot per scenario) of ns/entity vs entity count."""
    fig, axes = plt.subplots(
        2, 4,
        figsize=(17, 8),
        sharex=True,
    )
    axes = axes.flatten()

    for i, s in enumerate(scenarios):
        ax = axes[i]
        for e in engines:
            ys = [stats_min.get((e, s, n), 0) for n in counts]
            if all(y == 0 for y in ys):
                continue
            style = ENGINE_STYLE[e]
            ax.plot(
                counts, ys,
                marker="o",
                markersize=4,
                color=style["color"],
                linewidth=style["lw"],
                label=style["label"],
            )

        ax.set_xscale("log")
        ax.set_yscale("log")
        ax.set_title(s, fontsize=10)
        ax.grid(True, which="both", alpha=0.25)
        ax.set_xlabel("entities")
        ax.set_ylabel("ns / entity")

    # 8th cell is unused — repurpose it as the legend and metadata pane.
    axes[-1].axis("off")
    handles, labels = axes[0].get_legend_handles_labels()
    axes[-1].legend(handles, labels, loc="center", fontsize=11, frameon=False,
                    title="Engine", title_fontsize=12)
    axes[-1].text(
        0.5, 0.05,
        f"baseline captured {datetime.now():%Y-%m-%d}\n"
        f"min of 5 runs, lower is better",
        transform=axes[-1].transAxes,
        ha="center", va="bottom",
        fontsize=9, color="#666666",
    )

    fig.suptitle(
        "ECS Benchmark Scaling — ns / entity vs entity count",
        fontsize=14,
    )
    plt.tight_layout()
    plt.savefig(out_path, dpi=130)
    plt.close()


def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_CSV
    if not os.path.exists(csv_path):
        print(f"CSV not found: {csv_path}", file=sys.stderr)
        sys.exit(1)

    runs = load(csv_path)
    if not runs:
        print(f"no rows in {csv_path}", file=sys.stderr)
        sys.exit(1)

    stats_min = mins(runs)
    stats_med = medians(runs)

    # Discover engines and counts actually present, but keep canonical order.
    present_engines = {e for (e, _, _) in runs.keys()}
    engines = [e for e in ENGINE_ORDER if e in present_engines]
    counts  = sorted({c for (_, _, c) in runs.keys()})

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    summary_path = os.path.join(OUTPUT_DIR, "baseline_1M.png")
    scaling_path = os.path.join(OUTPUT_DIR, "scaling.png")

    plot_summary_1M(stats_min, stats_med, engines, SCENARIOS, summary_path)
    plot_scaling(stats_min, engines, SCENARIOS, counts, scaling_path)

    print(f"wrote {summary_path}")
    print(f"wrote {scaling_path}")
    print(f"  source: {csv_path}  ({sum(len(v) for v in runs.values())} runs across "
          f"{len(present_engines)} engines × {len(SCENARIOS)} scenarios × {len(counts)} counts)")


if __name__ == "__main__":
    main()
