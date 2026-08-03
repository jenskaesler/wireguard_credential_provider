# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning follows the scheme `<Year>.<Month>.<Release>`.

---

## [2026.8.2] – 2026-08-03

### Fixed
- `dll.cpp`: Removed debug load marker writing to `C:\Windows\Temp` (DllMain + DllGetClassObject)
- `WireGuardProvider.cpp`: Removed `ProvLog` helper and all call sites writing to `C:\Windows\Temp`
- `helpers.h`: Removed all fallback log paths to `C:\Windows\Temp` – if INSTDIR log dir is unavailable, no log is written
- `helpers.h`: `ReadRegDword` now also accepts `REG_SZ` with decimal string (e.g. `"180"`) – no hex conversion needed in Registry Editor
- `WireGuardTray.cpp`: `WireGuardManager` service disabled on tray init to prevent wireguard.exe UI respawn
- `WireGuardTray.cpp`: Send `WM_CLOSE` before `TerminateProcess` so wireguard.exe can clean up its tray icon via `Shell_NotifyIcon(NIM_DELETE)`
- `installer/WireGuardCredentialProvider.nsi`: Migrate legacy `REG_DWORD` values (`LogLevel`, `LogRetentionDays`, `HandshakeTimeoutSec`) to `REG_SZ` on update
- `installer/WireGuardCredentialProvider.nsi`: `HKLM\Run` autostart replaced by `CommonStartup` shortcut with `RunAsAdministrator` flag – tray now starts as admin on login
- Silent install parameters added: `/LOGLEVEL`, `/HANDSHAKE`, `/THUMBPRINT`, `/SMARTCARD`, `/PINREQUIRED`, `/DISCONNECTREMOVE`, `/TILELABEL`, `/CONFIGDIR`
- All registry keys now written directly by installer (configure.reg removed)


---


## [2026.8.1] – 2026-08-01

### Added
- **Handshake timeout** – auto-disconnect when WireGuard handshake is too old
  - New registry key `HandshakeTimeoutSec` (REG_DWORD, 0 = disabled)
  - Pre-logon: checked every second in SC-Watch thread
  - Post-logon: checked every 10 seconds in Network-Watch thread
  - `LOG_CRIT` entry when disconnect is triggered (visible at LogLevel=1)
  - Balloon notification in tray on disconnect
  - `WGGetLastHandshakeSec()` in helpers.h: parses `wg show <profile> latest-handshakes`
- **Corporate network detection** – auto-disconnect when on domain network
  - `WGCPIsOnCorporateNetwork()` via `INetworkListManager` (NLA API)
  - Pre-logon CP: blocks Connect with status message when corp network detected
  - Post-logon Tray: auto-disconnect + balloon notification
- **Redesigned tray context menu**
  - Profiles shown directly (no submenu)
  - YubiKey status with serial number (S/N via `ykman info`)
  - Yubico Authenticator shortcut (auto-detected, launched with admin token)
  - Delete profile with confirmation dialog (elevated fallback)
  - Left click on tray icon toggles connect/disconnect
- **Improved tray tooltip** (hover)
  - WireGuard VPN / separator / status / profile / uptime / handshake age / traffic
  - Icons: 🟢🔴 status, 🖥 host, ⏱ uptime, 🔑 handshake, 🌐 traffic
  - Handshake age formatted as h/m/s
  - Compact traffic format (79.6M instead of 79.6 MB) to fit 128-char limit

### Changed
- `HandshakeTimeoutSec` registry key written by installer (not configure.reg)
- All registry keys written directly by NSI installer (configure.reg removed)
- New keys checked individually on update, added if missing
- `WGGetLastHandshakeSec`: uses stdout pipe instead of cmd.exe redirect
- Traffic format: `79.6M` / `43.2M` (compact, no space before unit)
- Tray manifest: `asInvoker` → `requireAdministrator`
- Log encoding: unified UTF-8, no separate `_cp.log` suffix

### Fixed
- Yubico Authenticator path corrected to `C:\Program Files\Yubico\Yubico Authenticator\authenticator.exe`
- Yubico Authenticator launched via `CreateProcess` (inherits admin token, works without UAC)
- Double space in tooltip `Handshake  ausstehend` → `Handshake ausstehend`
- Tray tooltip truncated at 128 chars (traffic line was cut off)
- Tooltip icon `\u1F512` corrected to `\U0001F511`


---


## [2026.7.31] – 2026-07-31

### Added
- **Pre-Logon YubiKey PIV authentication** fully functional
  - PIN entry field directly in the credential tile (`CPFT_PASSWORD_TEXT`, masked)
  - PIN extracted via `CredUnPackAuthenticationBufferW` in `GetSerialization`
  - Certificate read from YubiKey via PIV GET DATA APDU with GET RESPONSE chaining
  - RSA2048 certificates (>256 bytes) now fully retrieved via `SW=61xx` chaining
  - SELECT PIV AID sent before GET DATA (required for UICC/NFC context)
  - Separate CP log file: `C:\Windows\Temp\wgcp_ddMMyyyy_cp.log`
  - Log directory fallback to `C:\Windows\Temp` when SYSTEM cannot write to INSTDIR
- **`installer\Setup-YubiKey.ps1`** – interactive YubiKey PIV setup tool
  - Menu: Initialize / Register existing YubiKey / Export report
  - Full PIV reset, random 8-digit PIN/PUK, RSA2048 key + self-signed cert (10y)
  - AD Distinguished Name lookup (`Get-ADUser`) with manual fallback
  - Writes `SmartcardEnabled=1` and `SmartcardCertThumbprint` to registry
  - Setup report (PIN/PUK/thumbprint) saved to user-defined path

### Changed
- All user-facing "Smartcard" labels replaced with "YubiKey" (pre-logon tile and tray)
  - `Wrong smartcard` → `Wrong YubiKey`
  - `Smartcard detected` → `YubiKey detected`
  - `Please insert YubiKey / smartcard...` → `Please insert your YubiKey...`
  - `Smartcard Authentication` → `YubiKey Authentication`
- PIN field in pre-logon tile: shown when disconnected, hidden when connected
- `GetSerialization` triggers `CommandLinkClicked` when user presses Enter
- SC-Watch thread: detects card present at startup, updates status on every tick
- Log encoding changed from UTF-16LE to UTF-8 (no BOM, readable with standard tools)
- Default log level changed back to CRIT (1) – DEBUG only when explicitly configured
- Version bumped to 2026.7.31 in all RC files

### Fixed
- Pre-logon PIN was never received (`CPFT_PASSWORD_TEXT` does not call `SetStringValue`)
  → Fixed via `CredUnPackAuthenticationBufferW` in `GetSerialization`
- YubiKey RSA2048 certificate truncated at 258 bytes (standard APDU limit)
  → Fixed via GET RESPONSE chaining (`00 C0 00 00 Le`) for `SW=61xx` responses
- `SHCreateDirectoryExW` failure silently prevented all CP logging as SYSTEM
  → Fixed: error checked, falls back to `C:\Windows\Temp`
- CP and Tray wrote to same log file in `C:\Windows\Temp`
  → Fixed: CP uses `_cp.log` suffix, detected via `GetModuleFileNameW`
- `CERT_STORE_PROV_SMARTCARD` not available in modern SDK
  → Replaced with direct PIV GET DATA APDU
- `SCardFreeMemory(nullptr)` compilation error
  → Removed unnecessary reader status call
- Leftover `CredLog` call causing compilation error after debug cleanup


---


## [2026.7.30] – 2026-07-30

### Added
- **Post-Logon Tray Application** (`WireGuardCPTray.exe`) – replaces the WireGuard UI for managed machines
  - System tray icon with color-coded lock icons (green = connected, red = disconnected)
  - Context menu: connection status header, active profile line, connect/disconnect, profile submenu
  - Profile import: file dialog → elevated copy to WireGuard config directory
  - Config folder shortcut (opens as Administrator due to WireGuard ACLs)
  - Bilingual UI: German / English based on `GetUserDefaultUILanguage()`
  - Dark Mode support via `SetPreferredAppMode` (uxtheme.dll ordinals 133/135/136)
  - Autostart via `HKLM\Software\Microsoft\Windows\CurrentVersion\Run` (all users)
  - Single-instance guard via named mutex
  - Shell taskbar readiness wait loop on autostart (prevents silent `Shell_NotifyIconW` failure)
  - Smartcard / YubiKey PIV authentication gate before every tunnel connect (same code path as CP DLL)
  - WireGuard UI watcher thread: kills `wireguard.exe` if a visible window is detected (runs every 500ms, hides window before terminating to prevent flash)
  - Start Menu shortcut watchdog: removes WireGuard shortcut after updates; backs up before first removal
- **Installer overhaul** (`WireGuardCredentialProvider.nsi`)
  - WireGuard auto-install: silently downloads and installs WireGuard if not present (inetc plugin, TLS 1.2)
  - Component pages: **Simple** (CP only), **Full** (CP + all YubiKey tools), **Custom** (free selection)
  - YubiKey Authenticator (v7.4.1), Minidriver (latest), Manager CLI (v5.9.2) – optional downloads
  - Silent deployment parameters: `/S`, `/FULL`, `/YKAUTH`, `/YKMINI`, `/YKMGRCLI`
  - WireGuard Start Menu shortcut: backed up to `INSTDIR\backup\`, removed on install, restored on uninstall
  - Bilingual installer strings (German / English)
  - `QuietUninstallString` set in registry for MDM-compatible silent uninstall
- **Tray icons**: custom ICO files with alpha channel and multiple sizes (16×16, 32×32, 48×48)
- **Application icon** (`icon.ico`) embedded in `WireGuardCPTray.exe` (resource ID 1, visible in taskbar and autostart manager)
- **`resource.h`**: `IDI_TRAY_CONNECTED (103)` and `IDI_TRAY_DISCONNECTED (104)` for tray-specific icons

### Changed
- `helpers.h`: `WGCP_TRAY_BUILD` preprocessor guard splits CP-only headers from tray build
- `helpers.h`: `WGGetConfigDir()` replaces hardcoded `WG_CONFIG_DIR` define; reads `ConfigDir` from registry with fallback chain
- `helpers.h`: `WGEnumProfiles()` uses `*.dpapi` search pattern with manual extension check (Windows `FindFirstFileW` does not support compound extensions)
- `helpers.h`: `WGConnect()` validates config file and `wireguard.exe` existence before spawning, logs exit code, uses `CREATE_NO_WINDOW`
- `helpers.h`: `WGDisconnect()` now uses `CREATE_NO_WINDOW` and logs exit code (consistent with `WGConnect`)
- `helpers.h`: `WGIsTunnelConnected()` logs service name and current state on every call
- `helpers.h`: `WGGetConnectedSince()` returns bilingual label ("Verbunden seit" / "Connected since")
- `WireGuardCredentialProvider.nsi`: `ExePath`, `WgExePath`, `ConfigDir` always overwritten on install/update (not just first install)
- `build.bat`: extended with `TRAY_SRC` check and copy; WireGuard binaries no longer bundled in installer
- Registry key `HKLM\SOFTWARE\Jens Kaesler\WireGuard Credential Provider` is now the single source of truth for both components
- All source code comments converted to English throughout

### Fixed
- `WireGuardTray.h`: orphaned `// FIX: std::nothrow` comment removed; correct include annotations added
- `WireGuardTray.cpp`: dead first `EnumWindows` call in watcher thread removed (variable `bHasWindow` was unused)
- `WireGuardTray.rc`: ICO resources referenced by simple filename (files placed in `tray-app\resources\` next to the `.rc` file)
- `WireGuardTray.vcxproj`: `$(ProjectDir)resources` added to RC compiler include path
- NSIS: `$PROGRAMDATA` / `$COMMONAPPDATA` replaced with `ReadEnvStr` / `!define` workarounds (NSIS does not expand these natively with spaces in path)
- NSIS: `NSISdl` replaced with `inetc` plugin for all downloads (NSISdl does not support TLS 1.2/1.3)


---


## [2026.7.6] – 2026-07-29

### Changed
- Registry key moved to `HKEY_LOCAL_MACHINE\SOFTWARE\Jens Kaesler\WireGuard Credential Provider` (previously `SOFTWARE\WireGuardCredentialProvider`) – applies to the credential provider, shutdown service, installer, and `configure.reg`
- All source code comments, log messages, and UI strings fully translated to English
- Embedded icons updated to new connected/disconnected design
- `installer/README.md` translated to English

### Fixed
- `min()` call in smartcard PIN copy replaced with explicit ternary – `NOMINMAX` was defined, making `min()` unavailable
- Duplicate `SetStringValue` stub removed (caused linker error after previous refactor)

### Migration
Existing installations using the old registry key (`SOFTWARE\WireGuardCredentialProvider`) must re-run the installer or manually export and re-import the configuration under the new key path. The uninstaller cleans up the old key if present.

---

## [2026.7.14] – 2026-07-25

### Added
- Comprehensive debug logging across all smartcard functions:
  - `WGCPLoadSmartcardConfig` – logs all config values after loading
  - `WGCPFindSmartcard` – logs each reader checked and result
  - `WGCPWaitForCard` – logs wait start with timeout value
  - `WGCPIsCardRemoved` – logs card removal event
  - `WGCPVerifyCertThumbprint` – logs thumbprint being checked and match result
  - `WGCPAuthenticateSmartcard` – logs `SCardListReaders` errors
  - `SetStringValue` – logs PIN received (length only, never content)
  - `_UpdateScStatus` – logs every status change to file
  - `_DoSmartcardAuth` – logs auth start, PIN length, disabled case
  - `_ScWatchThreadProc` – logs auto-connect/disconnect with tunnel name

---

## [2026.7.13] – 2026-07-25

### Added
- **Smartcard / YubiKey PIV authentication** – optional second factor before connecting a tunnel
- PIN entry field (password type) on the credential tile – hidden when smartcard is disabled
- Smartcard status field on the credential tile – shows card state and error messages
- PIN verification via VERIFY APDU (ISO 7816-4, PIV slot 80h) using WinSCard API
- Optional certificate thumbprint validation via CryptoAPI (SHA-1)
- Auto-connect when card is inserted (`SmartcardConnectOnInsert`)
- Auto-disconnect when card is removed (`SmartcardDisconnectOnRemove`)
- Background watch thread monitors card presence every second
- Remaining attempt count displayed after a wrong PIN entry
- PIN locked detection (SW `69 83`) with user-visible message
- PIN securely zeroed from memory after use (`SecureZeroMemory`)
- Compatible with YubiKey 5 series, standard PIV smartcards, and any CCID device

New registry values (all disabled by default):

| Value | Description |
|---|---|
| `SmartcardEnabled` | Enable smartcard authentication |
| `SmartcardPinRequired` | Require PIN before connecting |
| `SmartcardPinMinLength` | Minimum PIN length |
| `SmartcardPinMaxAttempts` | Max failed attempts before warning |
| `SmartcardTimeout` | Seconds to wait for card |
| `SmartcardConnectOnInsert` | Auto-connect on card insert |
| `SmartcardDisconnectOnRemove` | Auto-disconnect on card removal |
| `SmartcardReaderName` | Restrict to a specific reader |
| `SmartcardCertThumbprint` | Expected SHA-1 certificate thumbprint |

---

## [2026.7.12] – 2026-07-24

### Changed
- General code cleanup and refactoring ahead of smartcard feature

---

## [2026.7.11] – 2026-07-24

### Fixed
- Installer: use `robocopy.exe` to copy DLL to `System32` (bypasses WOW64 filesystem redirection from 32-bit NSIS process)
- Installer: disable WOW64 filesystem redirector explicitly before System32 operations
- Installer: kill `LogonUI.exe` before copying DLL to release the file lock
- Installer: use `$WINDIR\System32` instead of NSIS `$SYSDIR` to avoid redirection
- Installer: replace `.reg` import with direct `WriteRegStr`/`WriteRegDWORD` calls for reliability
- Installer: enforce Administrator elevation via UAC manifest

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
- **Color-coded icons**: two separate BMP resources (`wireguard_connected.bmp`, `wireguard_disconnected.bmp`) embedded directly into the DLL
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
- **Automatic default profile**: searches for a configuration file matching the computer name (e.g. `PC01.conf.dpapi`)
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
