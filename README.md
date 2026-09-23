# AlfaRaceX

**AlfaRaceX (ARX)** is a modular embedded framework for CAN/UDS research, diagnostics,
telemetry and feature development on Alfa Romeo Giulia and Stelvio vehicles.

The framework is intentionally split into independent layers:

```text
Physical CAN interfaces
        |
        v
ARX CAN transport
        |
        v
Signal / UDS decoders
        |
        v
ARX VehicleState
        |
        +-- Telemetry
        +-- Diagnostics
        +-- Feature modules
        +-- External interfaces
```

The first reference target is an Alfa Romeo Stelvio MY20 2.2 JTDm 210 HP Q4 with
ZF 8-speed automatic transmission.

## Current baseline

The current release candidate is **0.9.0-rc1**. It is targeted at the existing
three-controller STM32F072 hardware and currently includes:

- C1 / C2 / BH runtime orchestration
- prioritized CAN transport with retry/deadline handling
- UDS and ISO-TP transport
- structured vehicle state and diesel telemetry database
- function-specific state machines and restoration logic
- 19-byte inter-controller protocol
- pedal-controller UART protocol
- persistent-settings compatibility
- dashboard/menu and telemetry scheduling
- CAN sniffer and ELM-compatible diagnostic core
- WS2812 rendering/encoding path
- STM32F072 target HAL/glue sources
- Debug/Release host regression gates and Cortex-M0 object compilation
- firmware size-gate tooling for the existing board memory map

The project is **software-complete for the 1.0 hardware target**, with C1/C2/BH ARM images linked and memory-gated. Promotion to **1.0.0 Stable** is held only by the physical validation checklist on the existing board and vehicle.

## Project principles

- **Modular**: feature modules do not own low-level CAN transport.
- **Data driven**: vehicle knowledge is kept in structured databases where practical.
- **Observable**: errors, queue usage and diagnostic outcomes are measurable.
- **Testable**: recorded traffic can be replayed without the vehicle.
- **Vehicle aware**: features depend on discovered capabilities, not assumptions.
- **Fail conservative**: experimental frame overrides are disabled by default.

## Dynamic Shift Indicator

The initial feature watches engine speed and drive mode, then generates the three
shift urgency levels used by the instrument cluster message family.

Default behavior:

- disabled in Natural
- enabled in Dynamic
- enabled in Race
- configurable RPM thresholds
- no forced change of the vehicle's real DNA state

A separate IPC-gating strategy can be added after a MY20 CAN capture identifies the
minimum display-enabling condition required by the cluster.

## Building the portable core

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The portable build deliberately contains no MCU vendor HAL. Platform-specific drivers
are added below `targets/` and connect to the core through the ARX transport interface.

## Repository layout

```text
include/arx/            public interfaces
src/core/               transport/config/gateway state
src/vehicle/            vehicle state and decoders
src/features/           independent feature modules
database/               CAN/UDS/vehicle knowledge
targets/                platform-specific integration
tools/                  host-side tooling
tests/                  regression tests
docs/                   architecture and engineering notes
```

## License

Apache License 2.0. See `LICENSE`.

## Disclaimer

AlfaRaceX is an independent research and development project.

Alfa Romeo, Giulia and Stelvio are trademarks of their respective owners.
AlfaRaceX is not affiliated with or endorsed by Alfa Romeo or Stellantis.


## Feature coverage

ARX now contains modular implementations for the existing functional families:
CAN capture/routing, Start/Stop, shift indicator, dashboard/telemetry, drive-style
control, diagnostic modes, ACC/HAS controls, DPF alerts, Q4/AWD control, brake
override, DTC operations, odometer display handling, seat-belt configuration,
parking functions, window comfort functions, exhaust flap control and ELM-compatible
host commands.

See [`docs/FEATURE_MATRIX.md`](docs/FEATURE_MATRIX.md) for maturity and validation
status. Experimental functions that are not yet sufficiently validated remain
disabled in the stable profile.


## Behavioral parity

The current development branch follows a parity-first rule. See `docs/PARITY_AUDIT.md` and `docs/FEATURE_MATRIX.md`.

## Function-by-function parity audit

Functional compatibility is tracked explicitly in
[`docs/FUNCTION_AUDIT.md`](docs/FUNCTION_AUDIT.md).

ARX does not label a feature complete merely because a module exists: frame format,
timing, state transitions, restoration behavior and tests are checked independently.

## STM32F072 hardware target

The repository now contains a concrete three-image hardware contract for C1, C2 and
BH on STM32F072, including CAN timing, half-duplex UARTs, slave reset/transceiver
sleep control, persistent-page addresses and WS2812 timer/DMA requirements.

See [`docs/TARGET_STM32F072.md`](docs/TARGET_STM32F072.md) and
[`docs/REPLACEMENT_GATE.md`](docs/REPLACEMENT_GATE.md).

## 0.5 compatibility work

The portable core now includes a complete ISO-TP engine, multi-bus diagnostic
transaction routing, deployed flash-layout compatibility, USB mode lifecycle, exact
LED meter filtering/brightness behavior and BRG WS2812 pulse generation.

These additions reduce the remaining replacement work to target integration and the
still-open items listed in `docs/REPLACEMENT_GATE.md`.

## 0.7 consolidation

The replacement target is now explicitly fixed to the installed hardware. Dashboard
menu compatibility, diesel telemetry decoding, performance statistics and portable
log export have been expanded; remaining 1.0 work is target linking and replay/bench
validation rather than a hardware redesign.

## 0.8 release hardening

The 0.8 line makes validation itself part of the product: assertions remain active
in both Debug and Release tests, warnings are fatal, Cortex-M0 compilation is a
mandatory gate, and role-specific firmware-size checks enforce the existing-board
Flash/RAM limits. See `docs/RELEASE_0.8.0_DEV.md`.


## 0.9 release candidate

The three STM32F072 role images now build and link with the real ARM GCC toolchain
against STM32CubeF0 on GitHub Actions. All existing-board memory gates pass:

- C1: 52,628 B Flash / 12,400 B RAM
- C2: 39,036 B Flash / 12,400 B RAM
- BH: 38,516 B Flash / 12,400 B RAM

Debug, Release, AddressSanitizer/UndefinedBehaviorSanitizer and Cortex-M0 compilation
gates are green. The next promotion is `0.9.0-rc1`; final `1.0.0` requires the
physical existing-board validation checklist in
[`docs/HARDWARE_VALIDATION_CHECKLIST.md`](docs/HARDWARE_VALIDATION_CHECKLIST.md).
