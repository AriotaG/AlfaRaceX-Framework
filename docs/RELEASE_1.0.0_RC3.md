# AlfaRaceX 1.0.0-rc3

This release candidate is a post-RC2 consistency and validation hardening release.
It does not change the validated RC2 diesel telemetry runtime contract.

## Corrections

- catalogues Dynamic Shift display forcing explicitly as an experimental-disabled feature
- locks that classification with a regression test
- documents the reserved Dynamic Shift configuration flag as intentionally unused by the Stable runtime
- aligns the hardware validation checklist with the 55-page diesel telemetry contract
- adds explicit validation checks for battery SoC DID `0x19BD`, battery current frame `0x41A`,
  paired pages and 500 ms telemetry polling
- corrects stale audit wording that still described STM32F072 target bindings as missing
- refreshes the release gate with the actual current ARM memory figures

## Validation scope

The physical-validation target remains the existing three-controller STM32F072
BACCAble hardware installed on the reference Alfa Romeo Stelvio MY20 2.2 JTDm
210 HP Q4.

Dynamic-mode shift display forcing remains outside the Stable gate until the MY20
IPC display-enabling condition is isolated and validated on the vehicle.

## Stable promotion gate

RC3 remains a prerelease. Promotion to `1.0.0` requires bench boot, passive vehicle
validation, controlled active-feature validation and soak testing with no blocker.
