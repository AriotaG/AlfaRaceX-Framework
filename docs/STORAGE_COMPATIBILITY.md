# Persistent-storage compatibility

ARX can read and write the deployed persistent layout directly.

## Pages

- settings: `0x0801F800`
- performance statistics: `0x0801F000`
- parameter visibility: `0x0801E800`
- page size: 2048 bytes

## Settings layout

The compatibility layout contains 32 half-word settings. Each value is stored at a
four-byte stride, matching the deployed layout.

ARX converts those 32 values to a typed `ArxRuntimeConfig` in RAM and can export the
typed configuration back to the same layout. This preserves rollback compatibility
while the replacement firmware is being validated.

## Parameter visibility

Visibility flags are packed MSB-first into 16-bit values and use the same four-byte
stride.

## Improvement

Feature code never accesses absolute flash addresses. All flash operations go through
`ArxStorageBackend`, which makes host testing possible and allows a later redundant
ARX-native configuration store without losing compatibility with the existing pages.


## Erased Flash

An erased half-word (`0xFFFF`) is not interpreted as boolean true.

For boolean slots, only stored values `0` and `1` override the compiled ARX fallback
profile. Invalid/erased values retain the defined default. Numeric fields such as
shift threshold, launch threshold and pedal amplification likewise retain their
default when the stored value is erased.

The C1 STM32 startup now reads the persistent settings before queueing the initial
configuration synchronization to C2 and BH.
