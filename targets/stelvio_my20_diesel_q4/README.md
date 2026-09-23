# Stelvio MY20 2.2 Diesel Q4 target

This folder contains target-specific integration for the initial reference vehicle.

The portable ARX core does not depend on MCU vendor libraries. Platform integration
will provide:

- CAN controller initialization
- bus timing
- RX callback wiring
- TX callback wiring
- persistent storage backend
- monotonic millisecond clock
- USB / serial transport where required

No platform-specific implementation is committed in the initial baseline until the
exact MCU/build environment is fixed and validated.
