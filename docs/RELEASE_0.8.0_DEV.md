# AlfaRaceX 0.8.0-dev

This development milestone hardens the path toward the first stable release on the existing STM32F072 three-controller hardware.

## New release gates

- Assertions remain active in host tests in both Debug and Release builds.
- Compiler warnings are fatal in the portable core and STM32 target builds.
- Release review includes a public-source cleanliness check.
- Cortex-M0 portable-object compilation is mandatory.
- Target linking uses section garbage collection.
- Post-link firmware-size checks enforce the existing board memory map:
  - C1: 96 KiB application Flash region.
  - C2: 60 KiB application Flash region.
  - BH: 60 KiB application Flash region.
  - All roles: 16 KiB SRAM.
- Stable-release target builds reserve at least 2 KiB Flash headroom.
- CI is defined for Debug/Release host tests, Cortex-M0 object checks and all three STM32F072 role images.

## Hardware policy

No hardware replacement is required or planned for the 1.0 line. MCU, CAN topology, UART links, GPIO assignments, USB FS and existing wiring remain fixed compatibility constraints.

## Promotion status

0.8.0-dev is a software validation milestone. Promotion to 1.0.0 still requires successful C1/C2/BH target linking and hardware-in-loop/on-vehicle validation using the existing board.
