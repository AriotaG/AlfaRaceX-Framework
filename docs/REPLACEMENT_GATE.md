# Replacement release gate

A release may be described as a complete drop-in replacement only after every gate
below is green.

| Gate | Current status |
|---|---|
| Portable core compiles with warnings enabled | PASS |
| Function-by-function behavior audit | AUTOMATED PARITY COMPLETE; hardware replay/vehicle validation pending |
| C1/C2/BH hardware profiles | PASS |
| CAN timing/pins | PASS |
| Inter-controller 19-byte protocol | PASS (portable layer) |
| Pedal UART protocol | PASS (portable layer) |
| Low-consume timing and power-control contract | PASS (portable layer) |
| STM32F072 HAL initialization source | PASS — ARM GCC linked for C1/C2/BH |
| Runtime C1/C2/BH orchestration | PASS (host tested) |
| STM32 CAN/UART callback glue | PASS — ARM GCC linked for C1/C2/BH |
| Existing-board linker budgets (C1 96 KiB, C2/BH 60 KiB) | PASS |
| Flash/settings compatibility layer | PASS (portable + HAL backend source) |
| Boot-time persistent configuration load | PASS — compiled into target images |
| Exact dashboard/menu behavior | PORTABLE PARITY; target build PASS; vehicle replay validation pending |
| Full telemetry/UDS parameter set | DIESEL PARITY; target build PASS; page scheduler replay pending |
| Sniffer binary data path | PASS — USB target compiles/links; hardware USB lifecycle validation pending |
| Complete ELM-compatible bridge | PASS — portable core + USB target linked; hardware USB validation pending |
| WS2812 exact data path | PASS — renderer + PWM + TIM1/DMA target linked; hardware signal validation pending |
| USB CDC target | PASS — STM32CubeF0 linked; hardware enumeration validation pending |
| ARM cross-build C1 | PASS — 52,628 B Flash / 12,400 B RAM |
| ARM cross-build C2 | PASS — 39,036 B Flash / 12,400 B RAM |
| ARM cross-build BH | PASS — 38,516 B Flash / 12,400 B RAM |
| Bench test on existing hardware, no modification | PENDING |
| Passive in-vehicle validation | PENDING |
| Controlled active-feature validation | PENDING |

All software/build gates are complete. The project remains a prerelease until the
three physical gates (bench, passive vehicle, controlled active validation) pass on
the existing hardware. Only then may it be promoted to 1.0.0 Stable and described
as a validated drop-in replacement.
