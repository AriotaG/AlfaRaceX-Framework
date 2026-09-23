# AlfaRaceX 0.9.0-rc1

This is the first complete firmware release candidate for the existing three-controller
STM32F072 board.

## Automated validation

The candidate passes:

- 21/21 host tests in Debug
- 21/21 host tests in Release
- 21/21 host tests under AddressSanitizer + UndefinedBehaviorSanitizer
- Cortex-M0 freestanding object compilation with warnings as errors
- real `arm-none-eabi-gcc` link for C1
- real `arm-none-eabi-gcc` link for C2
- real `arm-none-eabi-gcc` link for BH
- post-link Flash/RAM gates for the existing board

## Measured firmware size

| Role | Flash used | Flash region | Headroom | RAM used | RAM | Headroom |
|---|---:|---:|---:|---:|---:|---:|
| C1 | 52,628 B | 98,304 B | 45,676 B | 12,400 B | 16,384 B | 3,984 B |
| C2 | 39,036 B | 61,440 B | 22,404 B | 12,400 B | 16,384 B | 3,984 B |
| BH | 38,516 B | 61,440 B | 22,924 B | 12,400 B | 16,384 B | 3,984 B |

## Hardware policy

No hardware change is required or planned. The candidate targets the existing
C1/C2/BH topology, pinout, CAN rates, UART links, USB FS, flash pages and WS2812
connection.

## Promotion to 1.0.0

The software and build gates are complete. The remaining mandatory gate is physical
validation on the existing board and vehicle using
`docs/HARDWARE_VALIDATION_CHECKLIST.md`.

For this reason this build is deliberately labeled `0.9.0-rc1`, not `1.0.0`.
