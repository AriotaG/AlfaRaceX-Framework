# Diagnostic bridge architecture

The diagnostic interface is split into three independent pieces:

1. command interpreter,
2. ISO-TP transaction engine,
3. bus router / inter-controller bridge.

This separation prevents USB parsing, vehicle transport and multi-bus routing from
sharing mutable state.

## Interpreter compatibility

The portable interpreter implements the commonly used CAN-oriented command set,
including strict identification strings, E/L/H/S/V, CAF, CFC, adaptive timing,
timeouts, protocol selection, programmable USER1/USER2 divisors, headers, filters,
masks and configurable Flow Control.

The default diagnostic protocol is ISO 15765-4 CAN 11/500 with automatic selection,
therefore DPN reports `A6` after reset.

## ISO-TP

The transport supports:

- Single Frame requests and responses
- First Frame requests and responses
- Consecutive Frames
- sequence-number checking
- Flow Control CTS / WAIT / overflow handling
- Block Size
- STmin
- payloads up to 255 bytes
- variable DLC for Single Frame requests
- 0xAA padding for formatted traffic

## Multi-bus routing

The master tries a cached ECU bus directly when known. For an unknown ECU the normal
candidate order is:

- C1
- C2
- BH

When the selected protocol requires a slower CAN rate, the search starts from BH.

The discovered ECU-to-bus association is retained by the router so subsequent
requests do not need to probe all networks.
