# Performance statistics and Max Hold

## 0-100 km/h

Timing starts when speed crosses from below `0.0625 km/h` to at least that value.
The timestamp receives the same 10 ms sample-period compensation used by the
reference behavior.

The run completes at 100 km/h and is marked missed after 20 seconds.

## 100-200 km/h

Timing starts when speed crosses from `<=100` to `>100 km/h`, with the same 10 ms
compensation. It completes at 200 km/h and is marked missed after 40 seconds.

Best values are updated only when the new completed time is lower. Persistence is
left to the platform/configuration layer.

## Max Hold

Max Hold is volatile by design. Enabling it resets the two displayed hold slots; a
slot then stores the greatest observed value until navigation/reset reinitializes it.
