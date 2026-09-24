# AlfaRaceX function-by-function parity audit

This document is the engineering gate for functional parity. A feature is marked
`PARITY` only when its observable behavior, message format, timing and restoration
logic are represented in ARX and covered by deterministic tests.

## Status legend

- `PARITY`: behavior is represented in the portable ARX core.
- `PARITY / TARGET`: portable logic and target binding are represented; physical target/vehicle validation is still required.
- `AUDIT`: behavior has been identified but the ARX implementation still needs work.
- `REFERENCE EXPERIMENTAL`: the reference behavior is itself explicitly incomplete.

| Function | Status | ARX implementation / finding |
|---|---|---|
| Smart Start/Stop | PARITY / TARGET | 10 s boot grace, 5 s engine grace, 0x226 status check, 0x4B1 button transform |
| Shift indicator | PARITY / TARGET | base threshold +500/+1000, 0x2ED urgency, optional MY23 IPC bit |
| Dynamic Shift display extension | REFERENCE EXPERIMENTAL | ARX-only extension; catalogued as disabled. The MY20 IPC-only display gate is not yet identified or vehicle-validated |
| Pedal controller | PARITY / TARGET | Auto/Bypass/A/N/D/R/Hybrid/Kids, +/-10, CRC, reply tracking, retry |
| DPF regeneration alert | PARITY / TARGET | mode 2 start, visual dirty-bit overlay, 11-sample zero-mode debounce |
| CAN route service | PARITY / TARGET | one-shot standard/extended capture with 0x18DABAF1 request and 0x18DAF1BA response |
| DYNO | PARITY / TARGET | session -> read status -> toggle write; 4 s timeout; 500 ms tester present |
| Q4/AWD control | PARITY / TARGET | explicit session/tester/IO/reset sequence and 30 ms hold refresh |
| Front brake override | PARITY / TARGET | session/tester/IO alternating cycle, one-shot ReturnControl, launch torque release hook |
| Seat-belt alarm config | PARITY / TARGET | IPC session + DID 0x55A0 IO Control, 10 s timeout |
| DTC read (Body) | PARITY / TARGET | extended session, 0x19/0x02/0xFF, SF + FF/CF ISO-TP, 20-record table |
| DTC clear | PARITY / TARGET | 0x14/FFFFFF request builder with target ECU address |
| Odometer blink mask | PARITY / TARGET | BH 0x356 SysEOL bit suppression |
| QV exhaust flap | PARITY / TARGET | session/tester/open-IO alternating cycle; ReturnControl on release |
| Comfort windows | PARITY / TARGET | lock/unlock timing, one-click and multi-click semantics, CRC rebuild |
| ACC virtual pad | PARITY / TARGET | CC->ACC, RES->distance, counter + CRC |
| HAS virtual pad | PARITY / TARGET | five-frame virtual button press path |
| ACC autostart | PARITY / TARGET | stationary/braking gating, RES or gentle-up burst |
| Front park mute | PARITY / TARGET | brake-travel threshold 14.5, PDC beep/LED/reverse state, push/release |
| Race display mask | PARITY / TARGET | 0x384/0x46C/0x4AF transforms represented |
| ESC/TC drive-style inversion | PARITY / TARGET | C1/C2 0x384, C1/C2 0x4AF, C1 0x5A8, BH 0x46C/0x25A, lane gesture and 50 ms C1 keepalive represented; inter-node state sync uses the 19-byte link |
| Park mirror | PARITY / TARGET | reverse+indicator selection, N transient, delayed restore, 0x5A6 capture, 0x5A8 command |
| Generic CAN router | PARITY | generic reusable rule engine |
| Runtime config | IMPROVED / TARGET | version + CRC, dual-slot model and STM32F072 storage backend are present; physical persistence validation remains |
| TX queue | IMPROVED | prioritized queues; dequeue only after success/expiry/retry exhaustion |
| CAN sniffer | PARITY / TARGET | 16-byte binary records, 256-byte ring, overflow marker, 64-byte chunking and 20 ms partial flush represented; CDC target and BH/C2 MSC<->CDC class switching are runtime-wired; physical enumeration/lifecycle validation remains |
| ELM-compatible interface | PARITY / TARGET | strict CAN-oriented AT compatibility, USER protocol divisors, filters, Flow Control, ISO-TP SF/FF/CF, route cache, CDC RX/TX and C1->C2/BH diagnostic relay are runtime-wired and end-to-end host-tested; hardware validation remains |
| Dashboard menu | PARITY / TARGET | 16-page main menu, 31-page setup menu, exact 55-page diesel parameter order, paired-value formatting, skip rules, setup cycles, USB mutual exclusion and 18-character rendering are represented; vehicle replay validation remains |
| Telemetry/UDS parameters | PARITY / TARGET | deployed/reference diesel page-pair contract, 500 ms active-page UDS scheduler, exact scaling decoder, battery SoC DID 0x19BD and native 0x41A battery current implemented. RC5 physical backup uses the audited reference firmware; physical CAN/IPC replay remains |
| LED strip | PARITY / TARGET | 46-LED patterns, raw pedal scaling, 0.9/0.1 filter, tangent brightness curve and 1,154-word BRG WS2812 encoding implemented; TIM1/DMA target is linked, signal validation remains |
| Flash/settings | PARITY / TARGET | deployed 32-slot settings, four-byte stride, visibility packing, statistics and BH mirror layouts supported through a testable backend; STM32 HAL backend added |
| Low-consume/wakeup | PARITY / TARGET | 3500/3400 ms timing, sleep blockers, UART pause/resume and PA4/PA5 action contract represented |
| Inter-controller link | PARITY / TARGET | deployed 19-byte 0x20-padded control frames, queue size 10 and 250/200 ms normal timing represented; checksummed 0x0E/0x0F/0x10 diagnostic frames are integrated with priority over normal queued traffic |
| Statistics / max-hold | PARITY / TARGET | 0-100/100-200 timing, 10 ms start compensation, 20/40 s timeout, best-time dirty state and two-value max hold implemented and runtime-wired |
| Save log to filesystem | IMPROVED / TARGET | portable CSV frame exporter implemented; FAT/MSC target sink remains to bind on the existing board |
| Remote start | REFERENCE EXPERIMENTAL | intentionally not promoted; reference implementation is commented/incomplete |
| Fuel-pump force test | REFERENCE EXPERIMENTAL | source contains a test path requiring separate security/UDS review |

## Improvement policy

ARX preserves externally visible behavior first. Improvements are allowed when they
do not silently change the feature contract. Typical improvements are:

- named states instead of numeric magic values,
- explicit timeouts,
- deterministic restoration paths,
- CRC and length validation,
- bounded buffers,
- sequence-number validation,
- test vectors,
- separation of portable logic from MCU HAL,
- metrics for retries, errors and timeouts.

Hardware-dependent parity is not claimed until the actual target layer provides the
same CAN controllers, UART, USB, GPIO, flash and low-power behavior.

| Runtime orchestration | PARITY / TARGET | role-specific C1/C2/BH RX dispatch, periodic scheduling, prioritized CAN TX and 19-byte inter-controller control are integrated and host-tested |
