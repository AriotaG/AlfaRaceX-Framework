# AlfaRaceX 1.0.0-rc5

RC5 is a hardware-parity and diagnostic-integration candidate based on direct
comparison with a backed-up physical three-controller STM32F072 reference unit.

## Physical reference findings

The audited installed firmware identifies the audited reference firmware version.

The backup confirms:

- STM32F07x / 128 KiB flash on C1, C2 and BH;
- separate C1/C2/BH application images;
- deployed persistent pages at the high-flash addresses already used by AlfaRaceX;
- USB Mass Storage on BH and C2 with VID 0x0483 / PID 0x572A;
- a small FAT volume on BH/C2 with VERSION.TXT;
- C1 does not expose that same legacy MSC image.

The original backup is not distributed with AlfaRaceX.

## RC5 corrections

- restores role-specific USB behavior:
  - C1 owns CDC diagnostics/sniffer;
  - BH/C2 default to read-only MSC;
  - BH/C2 switch safely to CDC for sniffer use and return to MSC afterward;
- adds a generated read-only AlfaRaceX FAT12 volume with VERSION.TXT for BH/C2;
- fixes USB class changes so a running class is detached/deinitialized before the
  replacement class is attached;
- wires CDC receive data into the ELM-compatible interpreter;
- wires ELM replies back to USB CDC;
- integrates the existing 19-byte diagnostic-link framing into the runtime;
- routes C1 diagnostic requests to C2/BH and relays CAN responses back to C1;
- prioritizes checksummed diagnostic-link frames over normal queued control traffic
  without dropping the normal queue;
- carries extended CAN-ID response-filter state across the diagnostic link;
- adds end-to-end host tests for ATI, local OBD traffic, C1->C2 diagnostic relay,
  MSC defaults and MSC<->CDC switching.

## Flashing safety

The application map remains unchanged:

- C1: 96 KiB from 0x08000000;
- C2/BH: 60 KiB from 0x08000000;
- persistent pages remain in the deployed high-flash locations.

The physical BH/C2 backup also contains a legacy FAT image beginning at 0x08010000,
outside the AlfaRaceX C2/BH application region. Do not mass erase. The AlfaRaceX
updater programs only pages touched by the selected HEX image.

## Validation status

RC5 must pass host Debug/Release tests, ASan/UBSan, the Cortex-M0 object gate and
real STM32F072 ARM builds for C1/C2/BH before publication.

Passing CI does not replace physical validation. Initial hardware testing must use
the existing board with active vehicle-control features disabled, followed by
passive CAN/USB/UART validation before enabling active features.
