# Dynamic Shift Indicator

## Objective

Expose the existing shift-request logic while keeping the real drivetrain drive mode
unchanged. A separate display-gating experiment targets Dynamic mode on MY20 IPC.

## Shift request

CAN `0x2ED`, byte 6 bits 1:0:

- `00` no request
- `01` urgency level 1
- `10` urgency level 2
- `11` urgency level 3

The parity baseline uses:

- Level 1: 4500 rpm
- Level 2: 5000 rpm
- Level 3: 5500 rpm

The base threshold remains runtime-configurable in 250 rpm steps.

The request generator itself is intentionally **not DNA-gated**. Display visibility is
a separate IPC concern.

For the MY23 IPC profile, byte 1 bits 6:5 are set to binary `10` when a shift request
is generated.

## Dynamic display gate

The MY20 cluster may suppress `0x2ED` unless an additional Race display context is
present. ARX keeps this separate from the shift generator so the final solution can
preserve:

- real DNA = Dynamic
- ECM / TCM / ESC semantics = Dynamic
- IPC graphics = Dynamic where possible
- shift recommendation = visible

Candidate display-context frames under differential capture are:

- `0x384`
- `0x46C`
- `0x4AF`
- `0x5A8`

No global Race conversion is part of the parity baseline.
