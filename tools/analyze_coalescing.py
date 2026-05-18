#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description="Analyze GPU coalescing from benchmark CSV.")
    parser.add_argument("--input", default=str(ROOT / "outputs" / "gpu_benchmark_results.csv"))
    parser.add_argument("--output-dir", default=str(ROOT / "outputs"))
    args = parser.parse_args()

    with Path(args.input).open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    csv_path = output_dir / "gpu_coalescing_results.csv"
    report_path = output_dir / "gpu_coalescing_report.txt"

    result_rows = []
    for row in rows:
        result_rows.append({
            "benchmark": row["benchmark"],
            "transactions": row.get("global_memory_transactions", "0"),
            "coalescing_efficiency": row.get("coalescing_efficiency", "0"),
            "transactions_per_warp_memory_instruction": row.get("global_memory_transactions", "0"),
        })

    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(result_rows[0].keys()))
        writer.writeheader()
        writer.writerows(result_rows)

    sorted_rows = sorted(result_rows, key=lambda row: float(row["coalescing_efficiency"] or 0))
    lines = [
        "GPU coalescing report",
        f"total requested bytes: see simulator stats output",
        f"total transferred bytes: see simulator stats output",
        f"worst coalescing instructions: {sorted_rows[0]['benchmark']}",
        f"best coalescing instructions: {sorted_rows[-1]['benchmark']}",
    ]
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {csv_path}")
    print(f"wrote {report_path}")


if __name__ == "__main__":
    raise SystemExit(main())
