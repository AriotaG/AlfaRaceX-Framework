#!/usr/bin/env python3
"""Report GCC .su records; individual frames are not whole-call-chain bounds."""
from pathlib import Path
import argparse


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    records = []
    for path in sorted(args.build.rglob("*.su")):
        for line in path.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            location, size, kind = line.rsplit("\t", 2)
            records.append((int(size), kind, location))
    if not records:
        parser.error("no compiler stack usage records found")
    records.sort(reverse=True)
    header = (
        "Compiler stack frames (bytes), descending. Includes vendor and unlinked functions.\n"
        "NOT a worst-case call-chain bound: recursion, indirect calls, interrupts and\n"
        "precompiled libraries require separate analysis and physical high-water measurement.\n"
    )
    report = header + "\n".join(f"{size}\t{kind}\t{name}" for size, kind, name in records) + "\n"
    args.output.write_text(report, encoding="utf-8")
    print(header, end="")
    for size, kind, name in records[:30]:
        print(f"{size}\t{kind}\t{name}")
    print(f"Recorded {len(records)} functions in {args.output}")


if __name__ == "__main__":
    main()
