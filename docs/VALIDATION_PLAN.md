# Validation plan

## Stage A — host

- compile with warnings enabled
- unit tests
- state-machine transition tests
- CRC vectors
- malformed-DLC tests
- queue overflow/retry tests

## Stage B — replay

Use real captures from the reference MY20 vehicle.

Golden captures should cover:

- ignition off/on
- Natural / Dynamic / All Weather
- manual shifting and automatic shifting
- DPF regeneration
- ACC engage / stop / resume
- reverse / parking sensors
- lock / unlock / window operation
- diagnostic sessions

## Stage C — passive vehicle validation

Run receive-only builds and compare decoded state against the instrument cluster and
diagnostic equipment.

## Stage D — controlled transmit validation

Enable one feature at a time. Validate exact target ECU, bus, period, timeout and
restoration behavior before enabling another active feature.

## Stage E — stable profile

Only validated combinations enter the stable vehicle profile.
