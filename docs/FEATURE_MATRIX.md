# AlfaRaceX feature matrix

`Core parity` means the portable ARX logic reproduces the validated reference
behavior at protocol/state-machine level. The STM32F072 C1/C2/BH target layer now
builds and links; statuses that mention target validation refer to physical
bench/vehicle validation, not missing target code.

| Feature | Module | Status |
|---|---|---|
| Smart Start/Stop | `arx_start_stop` | Core parity |
| Shift indicator | `arx_dynamic_shift` | Core parity |
| MY23 shift hint | `arx_dynamic_shift` | Core parity |
| Dynamic shift display experiment | `arx_drive_style` / `arx_dynamic_shift` | Experimental disabled; MY20 IPC-only display gate still to isolate and validate |
| ESC/TC customizer | `arx_drive_style` | Core parity |
| Race display mask | `arx_drive_style` | Core parity |
| Dyno | `arx_dyno` | Core parity |
| Q4/AWD control | `arx_awd` | Core parity |
| Front-brake override / launch assist | `arx_brake_override` | Core parity |
| ACC virtual pad | `arx_acc` | Core parity |
| ACC autostart | `arx_acc` | Core parity |
| HAS virtual pad | `arx_acc` | Core parity |
| DPF regeneration alert | `arx_dpf_alert` | Core parity |
| Read DTC | `arx_faults` | Core parity incl. ISO-TP receive |
| Clear DTC | `arx_faults` | Core parity per-bus sweep |
| Seat-belt alarm | `arx_seatbelt` | Core parity |
| Odometer blink mask | `arx_odometer` | Core parity |
| Route-message service | `arx_router` | Core parity |
| Diagnostic intrusion guard | `arx_immobilizer` | Core parity |
| Pedal controller | `arx_pedal_controller` | Core parity |
| Park mute | `arx_park_mute` | Core parity |
| Park mirror | `arx_park_mirror` | Core parity; compatible persistence backend implemented, target validation pending |
| Window comfort functions | `arx_windows` | Core parity |
| Exhaust flap | `arx_qv_exhaust` | Core parity |
| LED strip meter | `arx_led_strip` | Core parity; WS2812 PWM encoding and STM32 TIM1/DMA target source implemented, hardware validation pending |
| Low-consume policy | `arx_power` | Core parity; STM32 reset/transceiver GPIO contract implemented, hardware validation pending |
| CAN sniffer | `arx_sniffer` | Portable parity incl. binary buffering/streaming; role-aware CDC target linked, including temporary BH/C2 MSC->CDC switching; physical validation pending |
| ELM-compatible diagnostics | `arx_elm327` | AT interpreter, ISO-TP, CDC RX/TX and C1->C2/BH 19-byte relay are runtime-wired and host-tested; physical USB/CAN validation pending |
| Dashboard/menu | `arx_dashboard` | Diesel 55-page BACCAble order, 18-character value rendering, wheel/menu runtime and BH telematic scheduling implemented; vehicle replay/target validation pending |
| Remote start | experimental | Excluded from stable profile |
| Internal test-only functions | n/a | Excluded from stable profile |

See `PARITY_AUDIT.md` for the parity-first rule.


Detailed parity tracking: [`FUNCTION_AUDIT.md`](FUNCTION_AUDIT.md).

| Performance statistics | `arx_performance` | Core parity for 0-100 / 100-200 + best update |
| Max Hold | `arx_max_hold` | Core parity |
| Runtime configuration | `arx_config` | 32-slot migration + improved dual-slot CRC persistence model |
| Diesel telemetry database | `arx_telemetry_db` | 55-page deployed/reference diesel contract implemented, including paired pages, 500 ms UDS polling, battery SoC (0x19BD) and native battery current (0x41A). The physical unit audited for RC5 identifies its installed firmware as BACCAble V3.2.4; physical vehicle validation remains pending |

Hardware target contract: [`TARGET_STM32F072.md`](TARGET_STM32F072.md).
