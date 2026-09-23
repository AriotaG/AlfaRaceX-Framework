# Pedal Controller

The ARX pedal controller uses a dedicated half-duplex UART transport.

## Transport

- 9600 baud
- 8 data bits
- no parity
- 1 stop bit
- 9-byte command packets
- SAE J1850 CRC-8 in byte 8

## Modes

- Disabled
- Auto
- Bypass
- All Weather
- Natural
- Dynamic
- Race
- Hybrid Align
- Kids Limiter

## Map synchronization

The controller records the map-confirmation byte returned by the pedal unit and
retries the requested map when the actual and desired states do not match.

## Power adjustment

User adjustment is `-10 .. +10`. Each map uses its own scaling so the adjustment
preserves the intended shape and response range of that map.

## Auto mode

The target map follows the vehicle drive mode:

- All Weather -> A
- Natural -> N
- Dynamic -> D
- Race -> R

## Hybrid Align

Natural map is used for A/N/D and Race map for Race.

## Limiter mode

The conservative map is selected with minimum configured power. An additional
zero-demand command is available when the configured RPM or vehicle-speed limit is
exceeded.

The portable core builds and validates the protocol packets. The MCU-specific
half-duplex UART driver belongs to the target layer.
