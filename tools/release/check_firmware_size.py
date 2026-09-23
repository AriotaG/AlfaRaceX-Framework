#!/usr/bin/env python3
"""Enforce AlfaRaceX firmware size limits for the existing STM32F072 hardware."""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROLE_LIMITS = {
    "C1": {"flash": 96 * 1024, "ram": 16 * 1024},
    "C2": {"flash": 60 * 1024, "ram": 16 * 1024},
    "BH": {"flash": 60 * 1024, "ram": 16 * 1024},
}


def parse_size_output(text: str) -> tuple[int, int, int]:
    """Return GNU-size text/data/bss columns from the final data row."""
    rows = []
    for line in text.splitlines():
        m = re.match(r"^\s*(\d+)\s+(\d+)\s+(\d+)\s+\d+\s+[0-9a-fA-F]+\s+.+$", line)
        if m:
            rows.append(tuple(map(int, m.groups())))
    if not rows:
        raise ValueError("unable to parse GNU size output")
    return rows[-1]


def measure(elf: Path, size_tool: str) -> tuple[int, int, int]:
    proc = subprocess.run(
        [size_tool, str(elf)], check=True, text=True, capture_output=True
    )
    return parse_size_output(proc.stdout)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--role", choices=ROLE_LIMITS, required=True)
    ap.add_argument("--elf", type=Path)
    ap.add_argument("--size-tool", default="arm-none-eabi-size")
    ap.add_argument("--size-output", type=Path,
                    help="parse previously captured GNU size output instead of invoking a tool")
    ap.add_argument("--min-flash-headroom", type=int, default=2048)
    args = ap.parse_args()

    if bool(args.elf) == bool(args.size_output):
        ap.error("specify exactly one of --elf or --size-output")

    if args.size_output:
        text, data, bss = parse_size_output(args.size_output.read_text())
    else:
        text, data, bss = measure(args.elf, args.size_tool)

    flash = text + data
    ram = data + bss
    limits = ROLE_LIMITS[args.role]
    flash_headroom = limits["flash"] - flash
    ram_headroom = limits["ram"] - ram

    print(f"role={args.role} text={text} data={data} bss={bss}")
    print(f"flash={flash}/{limits['flash']} headroom={flash_headroom}")
    print(f"ram={ram}/{limits['ram']} headroom={ram_headroom}")

    failed = False
    if flash > limits["flash"]:
        print("FAIL: firmware exceeds flash region", file=sys.stderr)
        failed = True
    if ram > limits["ram"]:
        print("FAIL: firmware exceeds SRAM", file=sys.stderr)
        failed = True
    if flash_headroom < args.min_flash_headroom:
        print(
            f"FAIL: flash headroom below release reserve ({args.min_flash_headroom} bytes)",
            file=sys.stderr,
        )
        failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
