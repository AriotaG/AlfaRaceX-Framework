# Golden reference and current validation gates

The owner's three physical BACCAble dumps are the normative functional reference.
The embedded version string does not imply an obsolete firmware. Recovered source
and manual version numbers do not override verified behavior of these images.
The private binaries and third-party implementation sources are not distributed here.

The owner reported on 2026-09-26 successful AlfaRaceX DFU enumeration, open, full
131072-byte read, saved backup and hash for BH, C2 and C1. Each fresh backup matched
its golden image byte for byte (zero differences). This is owner-reported target
evidence for the read path, not evidence for programming, restoration or ARX vehicle parity.

Current transport regression tests establish host behavior only: ELM ignores foreign
bus/ID/IDE traffic, pending NRC does not terminate a raw request, CAN mailbox saturation
retains pending frames, and interchip timing survives tick rollover. The 19-byte
diagnostic extension checks its existing header, DLC, CAN ID and padding contract.
These tests are not labeled golden captures.

Target ingress is serialized into the main loop. Interrupts copy complete CAN,
interchip, pedal and USB events into a bounded queue. Queue access alone masks
interrupts; feature execution, storage writes and transport work run outside that
critical section. Full USB ingress holds the OUT packet and delays rearming instead
of silently dropping command bytes. Other rejected events are counted per source.
Runtime timing and capacity still require a physical stress check; host FIFO tests
and successful ARM builds do not establish wire-level equivalence.

No automatic physical write is authorized. Bench write/readback/boot/DFU re-entry/
golden restore/equality checks remain mandatory before vehicle validation.

Target builds default to `ARX_SAFE_BENCH_START=ON`. On every boot this applies a
volatile safety profile to both defaults and imported legacy preferences before
configuration synchronization. Active controls and automatic diagnostic polling
start disabled. Stored golden preferences are not rewritten by this policy; explicit
subsequent menu/configuration actions remain available. This deliberate ARX bench
deviation is separate from golden parity. Turning the build option off is not a
claim of physical validation and must not be used for an initial bench image.
