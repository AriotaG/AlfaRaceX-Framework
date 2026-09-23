# Runtime configuration

ARX preserves the functional meaning of the established configuration while changing
the persistence mechanism.

## Compatibility map

The historical configuration has 32 ordered values. ARX can import that layout into
typed fields so an existing setup can be migrated without re-entering every option.

The dormant historical remote-start slot is intentionally not promoted to the stable
configuration.

## Storage improvement

An ARX configuration block contains:

- magic
- schema version
- payload size
- monotonically increasing generation
- typed configuration payload
- CRC32

Two slots are used. At startup ARX selects the newest valid generation. A writer can
update the inactive slot first and leave the previous valid block untouched until the
new one is complete.

This changes storage robustness, not vehicle-visible behavior.

## Parameter visibility

Dashboard parameter visibility is packed MSB-first into 16-bit words, preserving the
established page ordering while keeping the API independent from the MCU flash driver.
