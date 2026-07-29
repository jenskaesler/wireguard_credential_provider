# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning follows the scheme `<Year>.<Month>.<Release>`.

---

## [2026.7.6] – 2026-07-29

### Changed
- Registry key moved to `HKEY_LOCAL_MACHINE\SOFTWARE\Jens Kaesler\WireGuard Credential Provider` (previously `SOFTWARE\WireGuardCredentialProvider`) – applies to the credential provider, shutdown service, installer, and `configure.reg`
- All source code comments, log messages, and UI strings fully translated to English
- `installer/README.md` translated to English

### Migration
Existing installations using the old registry key (`SOFTWARE\WireGuardCredentialProvider`) must re-run the installer or manually export and re-import the configuration under the new key path. The uninstaller cleans up the old key if it is present.

---

## [2026.7.5] – 2026-07-24

### Added
- Log rotation: `LogRetentionDays` (default: 7) – logs older than N days are automatically deleted on `Initialize`
- Log path supports date placeholder `ddMMyyyy` → daily log files (e.g. `wgcp_24072026.log`)
- `InstallDir` is written to the Registry by the installer and used as the base path for log files
- Installer creates a `logs\` subfolder in the installation directory
- `configure.reg` integrated into the installer (`installer/content/`) – imported automatically on first install
- Added `_DisconnectAllOnBoot` declaration in `WireGuardCredential.h` (fixes compiler error)

### Changed
- `LogLevel` default changed: `0` → `1` (CRIT) – critical errors are always logged
- Icons (connected/disconnected) reverted to the original WireGuard logo – cleanly RGBA-composited on green and red backgrounds respectively
- `deploy/` directory dissolved: `configure.reg` moved to `installer/content/`, `install.bat`/`uninstall.bat` removed (replaced by the installer)
- `.gitignore` updated

---

## [2026.7.4] – 2026-07-23

### Changed
- Field definitions (`g_rgFields`, `g_rgFieldStates`) extracted into central header `FieldDescriptors.h` – eliminates duplicate definitions in `WireGuardProvider.cpp` and `WireGuardCredential.cpp`
- `_bConnected`, `_bSelected` and `_bStopTimer` marked as `volatile` – ensures correct visibility between timer thread and UI thread
- Shutdown service uses `CREATE_NO_WINDOW` when launching `wireguard.exe` – prevents brief console window flash during shutdown
- `static_assert` in `FieldDescriptors.h` verifies at compile time that field count and `FI_NUM_FIELDS` are consistent

### Fixed
- Removed `_DisconnectAllOnBoot()` – the function incorrectly disconnected active tunnels immediately after connecting
- `WireGuardShutdownService` was disconnecting tunnels on service start instead of only on the `PRESHUTDOWN` event
- Fixed infinite loop caused by `CredentialsChanged` → `SetSelected` → `_UpdateFields` → `CredentialsChanged`: `NotifyStatusChanged()` is now called exclusively in `CommandLinkClicked` after an actual connect/disconnect action

---

## [2026.7.3] – 2026-07-23

### Added
- **WireGuardShutdownService**: standalone Windows service that listens for `SERVICE_CONTROL_PRESHUTDOWN` and cleanly disconnects all active WireGuard tunnels on PC shutdown
- Preshutdown timeout of 30 seconds configured – sufficient time to terminate all tunnels
- `install.bat` and `uninstall.bat` now install/uninstall both components (DLL + service) in a single step
- Manual test mode: `WireGuardShutdownService.exe /run` disconnects all tunnels without restarting

### Changed
- Shutdown listener window switched from `HWND_MESSAGE` to a visible top-level window (`WS_POPUP`, 0×0 pixels) – `HWND_MESSAGE` windows do not reliably receive `WM_ENDSESSION`

---

## [2026.7.2] – 2026-07-23

### Added
- **Automatic status refresh** every 5 seconds when the tile is selected (background thread)
- **Connection timer**: `⏱ Connected since HH:MM:SS` – reads the process start time of the tunnel service
- **Traffic statistics**: `↑ X MB ↓ Y MB` via `wg.exe show <profile> transfer` – shown only when connected
- **Color-coded icons**: two separate BMP resources (`wireguard_connected.bmp` green background, `wireguard_disconnected.bmp` red background) embedded directly into the DLL
- Icons integrated as project resources – no external files required at runtime
- `ICredentialProviderEvents::CredentialsChanged` called after connect/disconnect to force icon reload
- `wg.exe` path configurable via Registry value `WgExePath`

### Changed
- Disconnect command corrected: `/removetunnelservice` → `/uninstalltunnelservice` (correct WireGuard command name)
- Removed quotes around tunnel name for `/uninstalltunnelservice` – WireGuard expects the name without quotes
- After connect/disconnect: active polling for service status change instead of fixed `Sleep(2000)`
- `GetModuleHandleExW` anchor changed from member function pointer to static helper function

---

## [2026.7.1] – 2026-07-23

### Added
- **Profile dropdown (ComboBox)**: lists all `.conf.dpapi` configurations from the WireGuard configuration directory
- **Automatic default profile**: searches for a configuration file matching the computer name (e.g. `LT260430.conf.dpapi`)
- **Connect/Disconnect button**: `▶ Connect` / `⏏ Disconnect` – grayed out when no profile is available
- **Status display**: `● Connected` / `○ Disconnected` as a text field below the label
- Connect tunnel via `wireguard.exe /installtunnelservice <path-to-config>`
- Disconnect tunnel via `wireguard.exe /uninstalltunnelservice <tunnelname>`
- Connection status detection via Windows service `WireGuardTunnel$<ProfileName>`
- Log level and log path configurable via Registry (`LogLevel` DWORD: 0=off, 1=CRIT, 2=WARN, 3=DEBUG)
- Configurable icons for connected/disconnected state (`IconConnected`, `IconDisconnected`)

### Changed
- Tile click no longer launches an external program instance – connect/disconnect is handled directly via the WireGuard service mechanism
- `_UpdateFields` updates status text, traffic and button label without re-enumeration

---

## [2026.7.0] – 2026-07-23

### Added
- **WireGuard Credential Provider** as a Windows DLL (`ICredentialProvider` + `ICredentialProviderCredential`)
- Tile appears on the Windows login screen and lock screen (Logon + Unlock)
- Configuration entirely via Registry (`HKLM\SOFTWARE\WireGuardCredentialProvider`)
- Configurable path to `wireguard.exe` (`ExePath`)
- Configurable tile label (`TileLabel`)
- Configurable tile icon (`IconPath`, 128×128 px, 24bpp BMP)
- File-based logging with timestamp (`LogPath`, `LogLevel`)
- `regsvr32`-compatible registration/unregistration (`DllRegisterServer`/`DllUnregisterServer`)
- Installation scripts: `install.bat`, `uninstall.bat`, `configure.reg`
- Visual Studio 2022 project files (`.sln`, `.vcxproj`)

### Technical Foundation
- COM in-process server with `IClassFactory`
- Thread-safe reference counting via `InterlockedIncrement`/`InterlockedDecrement`
- Unicode throughout (`WCHAR`, `W`-suffix APIs)
- All strings via `StringCch*` family (no unsafe `strcpy`/`sprintf`)
- Resources via `CoTaskMemAlloc`/`CoTaskMemFree` per COM convention
