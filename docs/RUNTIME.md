# ARX Runtime

`ArxRuntime` is the orchestration layer between the portable feature modules and the
three physical controller roles.

## Receive path

Every received CAN frame follows one deterministic path:

1. record activity for power management;
2. optionally record the raw frame in the sniffer;
3. decode shared vehicle state;
4. update role-specific feature state;
5. apply only the transforms valid for the local vehicle bus;
6. queue generated frames in the prioritized TX transport.

Feature modules never call the MCU CAN HAL directly.

## Periodic path

### C1

The scheduler handles:

- Smart Start/Stop
- periodic drive-style message
- Q4/AWD maintenance
- exhaust flap control
- diagnostic intrusion guard
- seat-belt/DTC timeouts
- comfort-window actions
- pedal map synchronization / limiter override
- LED rendering
- C2/BH status polling

### C2

The scheduler handles:

- dyno state/Tester Present
- brake override / launch assist
- parking-sensor mute button simulation

### BH

The scheduler handles:

- park-mirror movement and restoration
- instrument-cluster telematic frames

All three roles can stream the binary sniffer through the active USB transport.

## Inter-controller path

The deployed 19-byte link is now consumed by the runtime, not by feature code.

C1 synchronizes configuration to C2/BH after startup. C2 owns the functional ESC/TC
toggle and sends the resulting display synchronization state back to C1/BH. This
keeps the functional drive-style path independent from the user's race-display
preference.

## Target loop

The STM32 target glue exposes:

- CAN FIFO callback -> `arx_runtime_on_can()`
- USART2 receive -> `arx_runtime_on_interchip()`
- USART1 pedal reply -> `arx_runtime_on_pedal_reply()`
- main loop -> `arx_runtime_tick()` + bounded TX drains

This is the first ARX layer that is structurally a firmware application rather than a
collection of portable feature modules.

## Inter-chip command identity

Some command byte values are intentionally reused by different destination classes.
The runtime therefore identifies a command by `(destination, command)`, never by the
command byte alone. This prevents collisions such as the shared `0x40` value used by
two distinct control paths.
