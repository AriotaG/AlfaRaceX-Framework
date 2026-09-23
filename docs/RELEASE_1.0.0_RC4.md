# AlfaRaceX 1.0.0-rc4

RC4 is a hardware-parity hardening release for the existing three-controller
STM32F072 target. It intentionally leaves the established CAN/UDS feature behavior,
persistent settings layout and diesel telemetry database unchanged.

## Inter-controller UART parity

- Restores byte-wise frame-start acquisition before completing the 19-byte
  inter-controller block.
- Accepts the complete deployed wire destination range 0x01..0x10 so normal
  controller traffic and diagnostic-link frames do not break framing.
- Invalid start bytes, UART errors, local half-duplex transmissions and wake events
  force re-synchronization from the next destination byte.
- Explicitly switches the single-wire UART from RX to TX and back around each
  transmission instead of transmitting while an interrupt receive remains armed.
- Applies the same explicit half-duplex direction handling to the C1 pedal UART.

## Boot / wake timing parity

C1 now waits 3502 ms after boot or slave wake before queueing the persisted feature
configuration for C2/BH. This matches the deployed firmware settling interval while
retaining the original 2000 ms inter-controller serial ignore window and the existing
250/200 ms master/reply timing.

## Scope

No PID/DID mappings, user defaults, persistent-memory addresses or active feature
algorithms were changed in RC4. The purpose of this candidate is to reduce
target-layer differences before the first physical AlfaRaceX vehicle validation.
