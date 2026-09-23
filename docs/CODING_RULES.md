# Coding rules

- C11 portable core.
- No feature may directly depend on MCU vendor HAL headers.
- No magic feature state numbers: use enums.
- No raw global cross-module mutable state.
- Every CAN decoder validates bus, ID and DLC before reading bytes.
- Every generated frame starts from an explicit template or validated source frame.
- TX queue removal happens only on successful send, expiry or exhausted retry policy.
- Experimental overrides are disabled by default.
- Configuration changes are versioned and integrity checked.
- New vehicle signals require a test vector or capture reference.
