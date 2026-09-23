# AlfaRaceX 1.0.0-rc1

This is the software-complete release candidate for the existing three-controller
STM32F072 board. No hardware replacement or wiring change is required.

## Automated validation

The candidate must pass before packaging:

- Debug host regression suite
- Release host regression suite
- AddressSanitizer + UndefinedBehaviorSanitizer suite
- Cortex-M0 object compilation with warnings as errors
- real ARM GCC link for C1
- real ARM GCC link for C2
- real ARM GCC link for BH
- post-link Flash/RAM budget enforcement for the existing board

## Existing-board memory budget

The target regions remain unchanged:

- C1 application region: 96 KiB
- C2 application region: 60 KiB
- BH application region: 60 KiB
- RAM: 16 KiB per controller

## Functional scope

The candidate includes the consolidated C1/C2/BH runtime and the stable functional
families implemented in the repository: CAN routing/capture, diagnostics/UDS/ISO-TP,
telemetry, Start/Stop, shift indication, DPF alerting, ACC/HAS controls, drive-style
synchronization, Q4/AWD control, brake/launch control, parking functions, comfort
windows, odometer display handling, seat-belt configuration, exhaust flap control,
pedal-controller communication, dashboard/menu, logging, persistent configuration,
USB diagnostic/sniffer paths and WS2812 output.

Experimental or incomplete reference-only functions remain excluded from the stable
profile.

## Stable 1.0 promotion gate

This candidate is intentionally marked prerelease. It becomes 1.0.0 Stable only
after the physical checklist in HARDWARE_VALIDATION_CHECKLIST.md is completed on the
existing board:

1. bench boot and peripheral verification for C1/C2/BH;
2. passive in-vehicle CAN/telemetry validation;
3. controlled feature-by-feature active validation;
4. soak testing across ignition, sleep/wake, USB and driving cycles.

A green automated build alone is not treated as proof of physical vehicle stability.
