# AlfaRaceX Updater

Windows single-file updater for AlfaRaceX firmware.

## Design

- branded AlfaRaceX-only user interface
- downloads the current firmware release-candidate manifest from a fixed HTTPS URL
- checks a separate updater manifest, so firmware releases do not require rebuilding the executable
- downloads all three Intel HEX images before flashing
- verifies SHA-256 before accepting each image
- validates every HEX address against the allowed application region
- talks directly to the STM32 ROM USB DFU interface through Windows WinUSB
- erases only 2 KiB flash pages touched by the selected image
- never performs mass erase, read-unprotect, option-byte writes or protection changes
- verifies programmed bytes through DFU upload before moving to the next module
- builds as one self-contained Windows x64 executable

## Module sequence

1. left USB port — BH
2. center USB port — C2
3. right USB port — C1

Only one module must be connected in DFU mode at a time.

## Windows USB prerequisite

The executable contains the updater logic and requires no external flashing
application. Windows must expose the DFU device through a WinUSB-compatible driver.
If it does not, the updater stops without modifying the device or the operating
system.

## Build

    dotnet publish AlfaRaceX.Updater/AlfaRaceX.Updater.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true


## Independent release cycle

The updater and firmware use separate version streams.

- Firmware releases update `distribution/release-candidate.json` only.
- The same updater executable reads that manifest at runtime and can install newer firmware releases.
- The updater is rebuilt only when files under `updater/**` or its build workflow change.
- Updater binaries are published under standalone tags such as `updater-v0.3.0`.
- `distribution/updater.json` advertises the latest updater version without triggering a rebuild.

This keeps firmware RC5, RC6, RC7, and later releases independent from the Windows updater binary.
