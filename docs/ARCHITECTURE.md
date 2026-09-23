# AlfaRaceX architecture

## Layers

### Transport

Owns CAN queueing, priorities, retry policy, deadlines and hardware callbacks.

Transport code knows nothing about vehicle features.

### Decoder

Consumes raw frames and updates a normalized `ArxVehicleState`.

A signal should be decoded in one place only.

### VehicleState

Represents the latest known vehicle state and validity of each signal.

Features consume `VehicleState`; they do not reimplement unrelated CAN bit extraction.

### Feature modules

Each feature owns:

- configuration
- explicit state
- timers
- generated messages
- feature-specific diagnostics
- failure state

### Platform targets

Target code binds the portable framework to the MCU, CAN peripherals, persistent
storage and USB/serial interfaces.

## TX policy

ARX uses three priorities:

- HIGH: time-critical diagnostic/control transactions
- NORMAL: feature-generated vehicle frames
- LOW: UI, telemetry and non-critical traffic

Queue entries are removed only after successful transmission or after retry/deadline
policy has been exhausted.

## Configuration

Configuration is versioned and CRC protected. Persistent backends should implement
two-slot storage so that a power loss during update does not destroy the last known
valid configuration.

## Experimental features

Experimental frame manipulation must be explicitly enabled and must remain isolated
from normal decoding. The default configuration should never silently activate an
experimental override.
