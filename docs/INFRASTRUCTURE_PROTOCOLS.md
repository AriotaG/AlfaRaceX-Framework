# Infrastructure protocols

## CAN sniffer stream

Each captured CAN message is represented by a fixed 16-byte record:

- byte 0: `0xA0 | DLC`
- bytes 1..3: timestamp milliseconds, 24-bit little-endian
- bytes 4..7: CAN ID, 32-bit little-endian
- bytes 8..15: payload, zero-padded

`0xAF` is reserved for an overflow record. The lost-frame count is stored in bytes
4..5.

The portable ring is 256 bytes (16 records). Flush chunks are capped at 64 bytes and
a partial chunk becomes eligible after 20 ms.

## Instrument-cluster text transport

The BH text channel uses standard CAN ID `0x090`, DLC 8.

An 18-character message is split across six frames, three ASCII characters per frame
at bytes 3, 5 and 7. Bytes 2, 4 and 6 remain zero. Frame-number bits are split across
bytes 0 and 1.

## ELM-compatible interface

The portable interpreter tracks:

- protocol declaration
- transmit header
- receive filter/mask
- output headers/spaces/linefeeds
- CAN auto-formatting
- automatic flow control
- adaptive timing
- response timeout
- custom flow-control header/data/mode

Actual CAN I/O remains behind the target transport so the same interpreter can route
to C1, C2 or BH without embedding MCU-specific code.
