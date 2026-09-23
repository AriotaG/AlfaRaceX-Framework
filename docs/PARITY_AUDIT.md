# Behavioral parity audit

AlfaRaceX uses a **parity-first** development rule:

1. reproduce the validated reference behavior,
2. isolate the behavior behind a named module,
3. add deterministic tests,
4. improve internals without silently changing the wire protocol,
5. introduce behavior-changing improvements only as explicit ARX options.

## Audit status

| Functional family | ARX module | Reference behavior represented | Improvement |
|---|---|---:|---|
| Smart Start/Stop | `arx_start_stop` | Yes | removes unused trigger variable; explicit boot/engine timers |
| Shift indicator | `arx_dynamic_shift` | Yes | threshold model separated from future IPC gating |
| MY23 shift hint | `arx_dynamic_shift` | Yes | explicit profile flag |
| DPF regeneration alert | `arx_dpf_alert` | Yes | named start/end hysteresis |
| Drive-style / ESC-TC transforms | `arx_drive_style` | Yes | per-bus transforms are isolated and testable |
| Race display mask | `arx_drive_style` | Yes | independent display-mask switch |
| Dyno toggle | `arx_dyno` | Yes | named response-driven states and NRC tracking |
| Q4/AWD control | `arx_awd` | Yes | exact wire sequence with named phases |
| Front-brake override | `arx_brake_override` | Yes | exact periodic sequence with separate launch logic |
| ACC / HAS controls | `arx_acc` | Yes, core behavior | separated gesture/input integration |
| DPF alert | `arx_dpf_alert` | Yes | explicit 11-frame end confirmation |
| Park mute | `arx_park_mute` | Yes | exact 50 ms push/release timing and LED-state reconciliation |
| Park mirror | `arx_park_mirror` | Yes, portable state logic | avoids unsigned-time-underflow trick for instant restore |
| Window comfort functions | `arx_windows` | Yes, core behavior | explicit phases |
| Pedal controller | `arx_pedal_controller` | Yes | typed modes, reply verification and deterministic retry |
| Exhaust flap | `arx_qv_exhaust` | Yes | named periodic states and double-click detector |
| Seat-belt alarm config | `arx_seatbelt` | Yes | response-driven UDS state machine and timeout |
| Odometer blink mask | `arx_odometer` | Yes | minimal isolated transform |
| Route-message service | `arx_router` | Yes | one-shot route request represented explicitly |
| Read DTC | `arx_faults` | Yes | ISO-TP SF/FF/CF reassembly and sequence checking |
| Clear DTC sweep | `arx_faults` | Yes | explicit per-bus sweep state |
| Diagnostic intrusion guard | `arx_immobilizer` | Yes, portable logic | named schedule rather than implicit globals |
| LED meter | `arx_led_strip` | Functional parity at renderer level | deterministic pattern generator; hardware DMA isolated |
| Low-consume policy | `arx_power` | Yes, policy level | GPIO/reset actions moved to target layer |
| CAN sniffer | `arx_sniffer` | Yes, stream/ring policy | blocking USB/HAL work remains target-side |
| ELM-compatible diagnostics | `arx_elm327` | AT state and request core expanded | cross-controller CAN bridge remains target-side |
| Dashboard/menu transport | `arx_dashboard` | BH text-frame transport represented | menu rendering kept separate |
| Hardware target | `targets/` | Not yet bound | STM32 target integration is the next flashability gate |

## Deliberately excluded from the stable profile

Features found only as inactive, commented, or test-only logic are not promoted to
the stable ARX feature set until they have a validated activation path.

This prevents dormant experiments from being mistaken for production behavior.

| Performance timing | `arx_performance` | Yes | explicit run states and storage-independent best update |
| Max Hold | `arx_max_hold` | Yes | typed volatile slots |
| Configuration persistence | `arx_config` | Functional meanings preserved | dual-slot generation + CRC32 |
