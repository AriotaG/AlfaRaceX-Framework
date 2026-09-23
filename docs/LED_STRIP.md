# LED strip controller

The target strip contains 46 WS2812-class LEDs.

## Vehicle input

The accelerator source byte is processed only for raw values >= 51. Once accepted,
the complete raw value is scaled by `100 / 180`, clipped to 24 and filtered:

`filtered = 0.9 * old + 0.1 * new`

The visual level is `round(filtered / 1.60)` and is clipped to 15.

## Layout

- LEDs 0..14: left dynamic section
- LEDs 15..30: center section, always full intensity
- LEDs 31..45: right dynamic section

The edge transition uses the original 0..45 tangent lookup curve rather than a simple
binary on/off boundary.

## Patterns

- gears 2 and 5: European pattern
- gears 3 and 6: randomized pattern
- all other gears: Italian pattern

## Wire encoding

The physical byte order is BRG. Each LED produces 24 PWM compare values, MSB first:

- logical zero: 17 timer ticks
- logical one: 34 timer ticks

Fifty zero-duty slots terminate the frame. For 46 LEDs the DMA frame contains 1,154
half-words.
