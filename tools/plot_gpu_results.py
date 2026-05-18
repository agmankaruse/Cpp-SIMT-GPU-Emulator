#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def numeric(row, key):
    try:
        return float(str(row.get(key, "0")).replace("%", ""))
    except ValueError:
        return 0.0


def write_report(rows, output_dir):
    path = output_dir / "gpu_benchmark_report.txt"
    lines = ["GPU benchmark summary", ""]
    for row in rows:
        lines.append(
            f"{row['benchmark']}: cycles={row['cycles']} IPC={row['IPC']} "
            f"coalescing={row['coalescing_efficiency']} divergence={row['branch_divergence_events']}"
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {path}")


def main():
    parser = argparse.ArgumentParser(description="Generate GPU benchmark plots or text reports.")
    parser.add_argument("--input", default=str(ROOT / "outputs" / "gpu_benchmark_results.csv"))
    parser.add_argument("--output-dir", default=str(ROOT / "outputs" / "plots"))
    args = parser.parse_args()

    with Path(args.input).open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    try:
        import matplotlib.pyplot as plt
    except Exception:
        write_report(rows, output_dir)
        return 0

    labels = [row["benchmark"] for row in rows]
    plots = [
        ("IPC", "GPU IPC by benchmark", "gpu_ipc.png"),
        ("coalescing_efficiency", "Coalescing efficiency", "gpu_coalescing.png"),
        ("branch_divergence_events", "Branch divergence events", "gpu_divergence.png"),
        ("scheduler_idle_cycles", "Scheduler idle cycles", "gpu_scheduler_idle.png"),
        ("L1_hit_rate", "L1 hit rate", "gpu_l1_hit_rate.png"),
        ("shared_memory_bank_conflicts", "Shared memory bank conflicts", "gpu_bank_conflicts.png"),
    ]
    for key, title, filename in plots:
        plt.figure(figsize=(10, 4))
        plt.bar(labels, [numeric(row, key) for row in rows])
        plt.title(title)
        plt.xticks(rotation=30, ha="right")
        plt.tight_layout()
        path = output_dir / filename
        plt.savefig(path)
        plt.close()
        print(f"wrote {path}")
    write_report(rows, output_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
