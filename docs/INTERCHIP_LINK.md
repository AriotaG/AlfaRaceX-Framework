# Inter-controller diagnostic link

The portable ARX link layer represents the 19-byte framing used to move diagnostic
traffic between the C1, C2 and BH controllers.

## Frame

- byte 0: destination
- byte 1: message type
- byte 2: flags
- bytes 3..6: CAN ID, big-endian
- byte 7: DLC
- bytes 8..15: payload
- byte 16: sequence number
- byte 17: XOR checksum of bytes 0..16
- byte 18: padding

Destinations:

- `0x0E` C2
- `0x0F` BH
- `0x10` C1/master

Message types cover CAN request, configuration, CAN response, end-of-response,
custom flow-control configuration and runtime arm/disarm.

Keeping the framing in `arx_link` allows the UART driver to remain a transport-only
target component and makes malformed-link frames testable on a host.
