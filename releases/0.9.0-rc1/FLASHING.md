# Flashing AlfaRaceX on the existing board

AlfaRaceX 1.0 is designed for the existing three-controller STM32F072 hardware. No
hardware modification is part of the 1.0 migration.

## Images

Use the image that matches the controller role:

- `AlfaRaceX-C1.hex` / `.bin` — C1 / 500 kbit/s master controller
- `AlfaRaceX-C2.hex` / `.bin` — C2 / 500 kbit/s controller
- `AlfaRaceX-BH.hex` / `.bin` — BH / 125 kbit/s controller

Never interchange role images.

## Before flashing

1. Preserve a working copy of the currently installed firmware for each role.
2. Record the current persistent configuration.
3. Verify that the target board is the existing AlfaRaceX-compatible STM32F072
   three-controller board and that the role has been identified correctly.
4. Use the same SWD/programming connection and power arrangement already used for
   firmware maintenance on this board.
5. Keep vehicle ignition off while programming.

## Programming regions

The replacement layout keeps the deployed memory map:

- C1 application region: 96 KiB from `0x08000000`
- C2/BH application region: 60 KiB from `0x08000000`
- visible-parameter page: `0x0801E800`
- statistics page: `0x0801F000`
- settings page: `0x0801F800`

Do not erase the persistent pages unless a configuration reset is intentionally
required. Prefer programming the application image over a full-chip erase.

## First boot

The first validation boot should be performed with active vehicle-control features
disabled. Confirm:

- normal boot and current consumption
- C1/C2/BH inter-controller communication
- passive CAN receive on the correct bus
- USB enumeration where applicable
- persistent configuration read
- sleep and wake behavior

Then enable one active feature at a time following
`docs/HARDWARE_VALIDATION_CHECKLIST.md`.

## Rollback

If any controller fails its validation step, stop active testing and restore the
previous working image for that same role. Do not continue by enabling additional
features on a controller whose basic boot/CAN/UART validation has failed.
