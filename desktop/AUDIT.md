# Desktop audit — 2026-09-25
Base: c7e6a11b2d96b43259b0de0e96ba427d56c6c257.

Reviewed repository tree (234 files), desktop shell/UI/history, shared updater services, DFU, firmware manifest, branding and Actions packaging. Embedded sources and CAN data are out of the change scope.

## Findings
- Reference uses a 278/1536-width sidebar, compact system panel, photographic header, horizontal module rows, global progress and dashboard log. Existing UI instead has a marketing hero, metric tiles, placeholder vector cars and a different ARX mark.
- WPF already has native window controls; retain them, add DPI declaration and guarded close during operations.
- Disclaimer UI and backend gate exist, but initial state enumerates USB and fetches manifest before acceptance. Gate starts hidden; bridge failure leaves no explanation. Packaged legal text is incomplete.
- Background DFU logging calls WebView2 without dispatcher marshalling.
- Flash has no automatic backup; restore does not compare against catalog hash when sidecar is absent.
- Imported backups reference external files; not copied into durable storage.
- Busy state disables flash/restore buttons without re-enabling them.
- Manifest refresh can invalidate the relationship between prepared firmware and displayed version.
- Manifest uses actual GitHub release URLs and hashes; no release catalog, retry or persistent download cache.
- SQLite backups/settings/events and readback verification are useful and retained.
- Smoke test checks SQLite only, not WebView2 or window behavior.
- Velopack packaging does not provide the requested directory/task wizard. Replace with Inno Setup.

## Verification boundary
This host is Linux, without attached STM32 device or Windows desktop. Browser tests can validate HTML/JS; GitHub Windows CI can compile/package and test installation. Physical DFU, interactive Windows focus/DPI and device role identification require real Windows/hardware validation. Do not claim those tests passed.
