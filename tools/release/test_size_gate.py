#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path

MODULE = Path(__file__).with_name("check_firmware_size.py")
spec = importlib.util.spec_from_file_location("check_firmware_size", MODULE)
mod = importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(mod)

sample = """   text    data     bss     dec     hex filename\n  50000     512    4096   54608    d550 arx_C2.elf\n"""
assert mod.parse_size_output(sample) == (50000, 512, 4096)
assert mod.ROLE_LIMITS["C1"]["flash"] == 98304
assert mod.ROLE_LIMITS["C2"]["flash"] == 61440
assert mod.ROLE_LIMITS["BH"]["flash"] == 61440
assert mod.ROLE_LIMITS["C1"]["ram"] == 16384
print("size gate self-test: PASS")
