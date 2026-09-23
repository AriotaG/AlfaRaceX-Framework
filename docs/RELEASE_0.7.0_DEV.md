# 0.7.0-dev consolidation milestone

This milestone keeps the installed hardware fixed and closes additional portable
replacement gaps.

## Added

- expanded diesel telemetry/UDS database through fuel-flow and MAF-temperature data;
- exact big-endian UDS response scaling with raw pre-scale offsets;
- portable 16-page main dashboard menu and 31-page setup menu;
- setup-value cycling for launch torque, shift RPM, pedal mode/power, ACC autostart,
  window modes, sniffer and diagnostic mode;
- sniffer/diagnostic USB mutual exclusion at configuration level;
- portable CSV CAN-log export sink;
- performance statistics and Max Hold wired into the C1 runtime;
- existing-hardware-only release policy and exact C1/C2/BH linker budgets.

## Still blocking 1.0.0

- vendor-HAL/CMSIS cross-link of all three images;
- USB CDC/MSC target binding;
- TIM1/DMA WS2812 target binding;
- complete steering-wheel/dashboard replay test;
- full UDS page scheduler replay;
- bench and in-vehicle validation on the installed hardware.
