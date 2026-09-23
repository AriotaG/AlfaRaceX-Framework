# Hardware validation checklist for 1.0

This checklist is the final gate between a software-complete release candidate and
the first stable 1.0 release. It uses the existing board; no hardware replacement is
required.

## Phase 1 — bench boot

For C1, C2 and BH separately:

- flash the correct role image
- confirm reset vector / normal boot
- verify no watchdog/reset loop
- verify current consumption is consistent with the existing firmware
- verify CAN transceiver state
- verify inter-controller USART2 at 38,400 baud
- verify C1 pedal USART1 at 9,600 baud
- verify C1 PA4/PA5 reset/sleep control
- verify C1 WS2812 output
- verify USB FS enumeration and disconnect/reconnect

## Phase 2 — passive vehicle validation

With transmission-producing features disabled:

- C1 receives 500 kbit/s traffic
- C2 receives 500 kbit/s traffic
- BH receives 125 kbit/s traffic
- RPM, speed, gear and DNA decode correctly
- oil / DPF / temperatures and other telemetry agree with diagnostic equipment
- menu text and steering-button events are correct
- persistent settings survive ignition cycles
- 3.5 s low-consumption entry and wake behavior are correct

No active feature is promoted if passive validation fails.

## Phase 3 — controlled active validation

Enable one feature at a time and verify its exact bus, frame, timing, restoration and
ignition-cycle behavior:

1. Smart Start/Stop
2. shift indicator
3. DPF alert
4. ACC/HAS virtual controls
5. parking mute
6. park mirror
7. comfort windows
8. seat-belt setting
9. DTC read/clear
10. dyno
11. Q4/AWD control
12. front-brake/launch assist
13. exhaust flap
14. pedal controller
15. ESC/TC display/control synchronization

For every feature confirm that disabling it restores normal ECU control.

## Phase 4 — soak

- repeated ignition cycles
- repeated sleep/wake cycles
- at least one long drive with logging
- DPF regeneration observation
- ACC stop/resume cycle
- reverse/parking cycle
- USB attach/detach cycles
- no queue growth, reset loop, bus-off storm or persistent DTC caused by ARX

## Stable promotion

`1.0.0` is promoted only after this checklist is completed without a blocker. Until
then the build remains a release candidate even if all automated tests are green.
