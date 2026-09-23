# Flash-memory compatibility

AlfaRaceX 1.0 targets the installed three-controller hardware without replacement.
The firmware therefore follows the same linker and persistent-data map already used
successfully on that board.

## Application regions

- C1: 96 KiB from `0x08000000`
- C2/BH: 60 KiB from `0x08000000`

## Persistent pages

- parameter visibility: `0x0801E800`
- statistics: `0x0801F000`
- settings: `0x0801F800`

These high pages are treated as an existing-hardware compatibility requirement.
The stable-release gate is not a new MCU: it is successful cross-linking inside the
same budgets followed by read/write validation on the current board.

No ARX feature may silently move or erase these pages during the replacement phase.
