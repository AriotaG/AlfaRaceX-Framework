# Replacement release gate

A release may be described as a complete drop-in replacement only after every gate
below is green.

| Gate | Current status |
|---|---|
| Portable core compiles with warnings enabled | PASS |
| Function-by-function behavior audit | IN PROGRESS |
| C1/C2/BH hardware profiles | PASS |
| CAN timing/pins | PASS |
| Inter-controller 19-byte protocol | PASS (portable layer) |
| Pedal UART protocol | PASS (portable layer) |
| Low-consume timing and power-control contract | PASS (portable layer) |
| STM32F072 HAL initialization source | IMPLEMENTED, cross-build pending |
| Runtime C1/C2/BH orchestration | PASS (host tested) |
| STM32 CAN/UART callback glue | IMPLEMENTED, cross-build pending |
| Existing-board linker budgets (C1 96 KiB, C2/BH 60 KiB) | PASS |
| Flash/settings compatibility layer | PASS (portable + HAL backend source) |
| Boot-time persistent configuration load | IMPLEMENTED; cross-build pending |
| Exact dashboard/menu behavior | PORTABLE PARITY; target replay validation pending |
| Full telemetry/UDS parameter set | DIESEL PARITY; page scheduler replay pending |
| Sniffer binary data path | PASS (portable layer); USB lifecycle pending |
| Complete ELM-compatible bridge | PASS (portable transaction core); USB target binding pending |
| WS2812 exact data path | PASS (renderer + PWM encoder); TIM1/DMA target start/callback pending |
| USB CDC/MSC target | PENDING |
| ARM cross-build C1 | PENDING |
| ARM cross-build C2 | PENDING |
| ARM cross-build BH | PENDING |
| Bench test on existing hardware, no modification | PENDING |
| Passive in-vehicle validation | PENDING |
| Controlled active-feature validation | PENDING |

Until the three cross-built images and target validation gates pass, the project must
not be presented as a drop-in replacement.
