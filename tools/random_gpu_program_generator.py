#!/usr/bin/env python3
import argparse
import random
from pathlib import Path


def generate(instructions, seed):
    rng = random.Random(seed)
    lines = [
        ".kernel random_gpu",
        "MOVI r1, 1",
        "MOVI r2, 2",
        "MOVI r3, 3",
        "MOV.LANEID r6",
        "SHL r7, r6, 2",
    ]
    ops = ["MOVI", "ADD", "ADDI", "SUB", "MUL", "SETP", "LD.GLOBAL", "ST.GLOBAL", "LD.SHARED", "ST.SHARED"]
    op_count = max(0, instructions - len(lines) - 5)
    for index in range(op_count):
        op = rng.choice(ops)
        dst = rng.randint(16, 31)
        if op == "MOVI":
            lines.append(f"MOVI r{dst}, {rng.randint(0, 31)}")
        elif op == "ADD":
            lines.append(f"ADD r{dst}, r1, r2")
        elif op == "ADDI":
            lines.append(f"ADDI r{dst}, r1, {rng.randint(-8, 8)}")
        elif op == "SUB":
            lines.append(f"SUB r{dst}, r3, r1")
        elif op == "MUL":
            lines.append(f"MUL r{dst}, r2, r3")
        elif op == "SETP":
            lines.append("SETP.LT p0, r1, r3")
            lines.append(f"BRA.P p0, rand_skip_{index}")
            lines.append(f"MOVI r{dst}, 0")
            lines.append(f"rand_skip_{index}: NOP")
        elif op == "LD.GLOBAL":
            address = rng.randrange(0, 1024, 4)
            lines.append(f"LD.GLOBAL r{dst}, [r0 + {address}]")
        elif op == "ST.GLOBAL":
            address = rng.randrange(4096, 8192, 4)
            lines.append(f"ST.GLOBAL [r0 + {address}], r1")
        elif op == "ST.SHARED":
            lines.append("ST.SHARED [r7 + 0], r1")
        elif op == "LD.SHARED":
            lines.append(f"LD.SHARED r{dst}, [r7 + 0]")
    lines.append("HALT")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description="Generate finite legal GPU assembly programs.")
    parser.add_argument("--instructions", type=int, default=100)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(generate(args.instructions, args.seed), encoding="utf-8")
    print(f"generated {output} seed={args.seed} instructions={args.instructions}")


if __name__ == "__main__":
    main()
