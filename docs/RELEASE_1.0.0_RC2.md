# AlfaRaceX 1.0.0-rc2

This release candidate hardens reference telemetry parity for the reference
Alfa Romeo Stelvio MY20 2.2 JTDm diesel Q4 target.

## RC2 corrections

- restores the exact 55-page diesel dashboard order used by the reference firmware
- restores paired dashboard pages rather than treating every signal as a separate page
- adds battery State of Charge through UDS DID `0x19BD`
- adds battery current decoding from native C1 frame `0x41A`
- keeps native `0x41A` SoC as a startup fallback until the first UDS SoC reply
- adds a bounded telemetry cache
- polls the active UDS dashboard page at the reference-compatible 500 ms cadence
- decodes the reply and pushes the formatted 18-character value string to BH
- restores reference-style formatting for numeric and enum dashboard values
- adds regression tests for the 55-page contract, battery decode, formatter and
  end-to-end UDS request/decode/dashboard path

## Automated validation

The RC2 source must pass:

- Debug host regression suite
- Release host regression suite
- AddressSanitizer + UndefinedBehaviorSanitizer suite
- Cortex-M0 object compilation
- real ARM GCC link and memory budget gate for C1
- real ARM GCC link and memory budget gate for C2
- real ARM GCC link and memory budget gate for BH

## Validation scope

RC2 is a physical-validation candidate for the reference MY20 diesel profile.
The gasoline dashboard profile is not part of this RC2 validation gate.

Dynamic-mode shift indication remains an experimental AlfaRaceX extension until the
MY20 IPC display-enabling condition is isolated and verified on the vehicle.

## Stable promotion gate

The exact candidate HEAD must pass the complete CI matrix before merge and packaging.
This remains a prerelease. It must not be promoted to 1.0.0 Stable until bench boot,
passive in-vehicle CAN/telemetry checks, controlled feature-by-feature validation and
soak testing pass on the existing reference hardware.
