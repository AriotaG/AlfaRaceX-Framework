# AlfaRaceX Desktop 0.2.0

.NET 8 / WPF / WebView2, local Bootstrap and Bootstrap Icons, persistent SQLite at `%LOCALAPPDATA%/AlfaRaceX/Desktop`.

## Behavior

- Native Windows caption controls, resizing, taskbar integration and PerMonitorV2 DPI declaration.
- Local Italian/English disclaimer gates USB and network operations; acceptance version is the SHA-256 of the packaged full legal text.
- Live GitHub release metadata, firmware asset validation, streamed downloads with cancellation, retries, SHA-256 and HEX address validation.
- Module state remains unknown when the DFU bootloader cannot report the installed version or physical role. USB connection is polled while idle.
- Flash creates and verifies a full backup, catalogs it, then uses the same USB handle for programming and readback verification.
- Imported backup files are copied into the data directory. Restore checks catalog SHA-256 and available sidecar metadata, followed by device readback verification.
- Backup notes and manual integrity checking. SQLite operation history remains after clearing the user log. User/technical log views, copy, export and folder access.
- Inno Setup wizard supports destination selection, Start Menu, optional Desktop shortcut, uninstall and downgrade rejection. User data is outside the installation directory. Microsoft-signed standalone WebView2 prerequisite is bundled for offline installation.

## Build and validation

The authoritative build is `.github/workflows/build-desktop.yml` on `work/desktop-production-ui`. It vendors the local UI dependencies, compiles and publishes a self-contained Windows x64 app, executes DOM and SQLite regression checks, downloads and validates the three real release firmware files, checks offline failure and cancellation, starts the actual WPF/WebView2 interface twice, captures screenshots, builds the installer and checks install/reinstall/uninstall data preservation.

Artifacts: `AlfaRaceX-Desktop-Setup-win-x64` contains `AlfaRaceX-Setup.exe` and SHA-256; `AlfaRaceX-UI-Validation` contains machine-readable results and actual app captures. Synthetic data is used only in isolated tests, never in the normal application UI.

To build locally: run `npm ci` under `AlfaRaceX.Desktop`, vendor Bootstrap and Bootstrap Icons as in the workflow, publish with `dotnet publish -c Release -r win-x64 --self-contained true -o publish`, acquire/verify the signed WebView2 prerequisite as in the workflow, then compile `installer/AlfaRaceX.iss` with Inno Setup 6.

## Remaining validation boundaries

No physical STM32/BACCAble was attached to the development or CI hosts. Hardware backup, flash, restore, disconnection recovery and driver compatibility still require device testing. DFU VID/PID does not establish module role; the operator must verify the physical port.

The code declares PerMonitorV2 and screenshots cover multiple window sizes; physical multi-monitor DPI transitions and interactive keyboard/focus behavior are not comprehensively certified by CI. Setup is not Authenticode-signed; the bundled Microsoft prerequisite is signature-verified.

For migration from Desktop 0.1.0, close and uninstall the previous Velopack application before using this installer. The new installer deliberately does not silently remove a separate previous installation. Preserve `%LOCALAPPDATA%/AlfaRaceX/Desktop`.

Firmware embedded code, CAN databases and released firmware were not modified. `main` is unchanged; this branch is a reviewable release candidate pending hardware validation.
