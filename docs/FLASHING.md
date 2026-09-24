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

A physical reference-firmware backup used for RC5 validation also contains the
BH/C2 legacy MSC image starting at `0x08010000`. That region is outside the
60 KiB AlfaRaceX C2/BH application image. The AlfaRaceX updater erases only pages
touched by the selected HEX image, so it does not require a full-chip erase and
does not touch the persistent pages.

Do not use mass erase. Do not erase the persistent pages unless a configuration
reset is intentionally required. Prefer the AlfaRaceX updater or page-scoped
programming of the matching HEX image.

## First boot

The first validation boot should be performed with active vehicle-control features
disabled. Confirm:

- normal boot and current consumption
- C1/C2/BH inter-controller communication
- passive CAN receive on the correct bus
- C1 USB CDC enumeration when diagnostics/sniffer are enabled
- BH/C2 default USB MSC enumeration and MSC<->CDC switching when sniffer is toggled
- persistent configuration read
- sleep and wake behavior

Then enable one active feature at a time following
`docs/HARDWARE_VALIDATION_CHECKLIST.md`.

## Rollback

If any controller fails its validation step, stop active testing and restore the
previous working image for that same role. Do not continue by enabling additional
features on a controller whose basic boot/CAN/UART validation has failed.
