# AlfaRaceX Desktop 0.2.0 — work in progress

Existing .NET 8/WPF/WebView2 shell with local Bootstrap, Bootstrap Icons and SQLite. The Windows window retains native caption buttons, resize and system shortcuts. Inno Setup replaces Velopack for the requested destination wizard, Start Menu, optional Desktop shortcut and uninstaller.

## Build

Run `npm ci` in `AlfaRaceX.Desktop`, copy the Bootstrap and Bootstrap Icons assets as specified in `.github/workflows/build-desktop.yml`, then publish the project on Windows and compile `installer/AlfaRaceX.iss` with Inno Setup 6. User data stays at `%LOCALAPPDATA%/AlfaRaceX/Desktop`.

## Validation status

- JavaScript syntax check: passed.
- DOM tests in `tests/ui.test.cjs`: passed (jsdom; this is not a visual or Windows test).
- Git whitespace check: passed after cleanup.
- Windows build, WebView2 runtime, installation, upgrade, uninstall, reinstall: NOT EXECUTED. Workflow prepared.
- Visual comparison and DPI: NOT EXECUTED; browser download failed in current environment.
- Physical backup/flash/restore: NOT EXECUTED; no device attached.
- New installer and installer SHA-256: NOT PRODUCED.

## Known remaining work before release

- Compile and resolve any Windows CI failures; test runtime provisioning on clean Windows without WebView2.
- Verify visual layout at reference size and Windows scaling. Three SVG viewports embed only the permitted brand/header regions of the supplied reference; UI controls remain real HTML/CSS/Bootstrap Icons.
- Finish structured operation history, optional backup notes, user/technical log filtering and recovery UX.
- Validate DFU flows physically. STM32 DFU VID/PID does not prove module role; the operator must verify the port. Installed firmware version is not claimed from USB enumeration.
- Validate migration from the old Velopack installation (the new installer does not uninstall it automatically), downgrade handling and product icon.
- Review cancellation during flash: readback verification and recovery require real hardware testing.

## Local commits / publication

Work branch: `work/desktop-production-ui`. Automatic approval review rejected the public GitHub push because the authorization in the attached brief was not accepted as trusted user text. No remote branch, pull request, build run or new release was created by this work.
