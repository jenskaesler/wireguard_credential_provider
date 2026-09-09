# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning follows the scheme `<Year>.<Month>.<Release>`.

---

## [2026.8.6] – 2026-09-09

### Added

- **Systray: Automatische Update-Prüfung** (`🔄 Auf Updates prüfen` im Kontextmenü)
  - Neuer umschaltbarer Menüeintrag mit Haken — aktiviert/deaktiviert die automatische
    Prüfung auf neue GitHub-Releases
  - Status wird in `HKLM\SOFTWARE\Jens Kaesler\WireGuard Credential Provider\AutoUpdateCheck`
    (DWORD) gespeichert — kann von Administratoren per Registry oder GPO gesteuert werden
    (Standard: 1 = aktiviert)
  - 10 Minuten nach dem Tray-Start wird
    `api.github.com/repos/jenskaesler/wireguard_credential_provider/releases/latest`
    per WinHTTP (HTTPS) abgefragt
  - Ist eine neuere Version verfügbar, erscheint ein Balloon-Tip:
    *„🔄 Update verfügbar: x.x.x — Hier klicken zum Download"*
  - Klick auf den Balloon öffnet direkt die GitHub-Release-Seite im Browser
  - Version wird mit installierter Version aus dem Windows-Uninstall-Key verglichen
    (`DisplayVersion`); `v`-Präfix im GitHub-Tag-Namen wird toleriert
  - Thread stoppt sofort wenn die Einstellung deaktiviert wird oder die App beendet wird

---

## [2026.8.5] – 2026-09-09

### Added

- **Systray: About-Dialog** (`ℹ️ Informationen...` im Kontextmenü)
  - Neue Menüoption vor „Beenden" öffnet einen modalen Dialog mit App-Name, Version
    (aus dem Windows-Uninstall-Registry-Key `DisplayVersion`), Copyright-Vermerk und
    Kurzbeschreibung
  - **GitHub-Link** als owner-drawn Button (blau, unterstrichen) öffnet
    `https://github.com/jenskaesler/wireguard_credential_provider` direkt im Browser
  - Neue Konstanten in `helpers.h`: `WGCP_REG_UNINSTALL`, `WGCP_GITHUB_URL`,
    `WGCP_VERSION_FALLBACK`
  - Implementierung analog zum bestehenden PIN-Dialog (programmatischer Win32-Dialog,
    keine `.rc`-Template-Abhängigkeit)

- **Systray: Profilwechsel in einem Schritt** (`⇄ Wechseln zu <Profil>`)
  - Neue Methode `_SwitchProfile(int profileIndex)` — trennt das aktive Profil und
    verbindet das gewählte in einer Aktion
  - Erscheint im Profil-Untermenü wenn ein anderes Profil bereits verbunden ist
  - Neue Menü-ID-Range `IDM_PROFILE_SWITCH_BASE 800`

- **Systray: YubiKey-Serial asynchron gecacht**
  - Seriennummer wird im `_SmartcardWatchThread` beim Einstecken der Karte via `ykman`
    ermittelt und in `_wszYkSerial` gespeichert
  - Menüaufbau blockiert nicht mehr (kein synchrones `WaitForSingleObject` beim Öffnen
    des Kontextmenüs)

- **Systray: Konfigurationsordner öffnen** — fehlender Menüeintrag nachgetragen
  (`📁 Konfigurationsordner öffnen...`); `_OpenConfigDir()` und `IDM_OPEN_CONFIG_DIR`
  waren bereits implementiert, aber nie ins Menü aufgenommen

### Fixed

- **Systray: Profilwechsel wurde ignoriert** — `MF_POPUP`-Eltern-Einträge senden kein
  `WM_COMMAND`; der bisherige `IDM_PROFILE_BASE+i`-Handler war toter Code. Jede
  „Verbinden"-Option im Untermenü erhält jetzt eine eindeutige ID aus dem Bereich
  `IDM_PROFILE_CONNECT_BASE+i`

- **Systray: Löschen war zu restriktiv** — „Löschen" wurde auch für
  ausgewählte-aber-nicht-verbundene Profile grau dargestellt. Neu: nur ausgegraut wenn
  das Profil aktiv verbunden ist

---

## [2026.8.4] – 2026-08-10

### Added

- `helpers.h`: **Two-stage corporate network detection** (`WGCPIsOnCorporateNetwork`)
  - Stage 1 (unchanged): NLA `DOMAIN_AUTHENTICATED` via `INetworkListManager`
  - Stage 2 (new fallback): detects domain-joined VMs and machines where NLA reports
    `PRIVATE` instead of `DOMAIN_AUTHENTICATED` (observed on Red Hat VirtIO / Hyper-V
    adapters because the DC was not reachable when NLA first classified the network at boot):
    1. Registry check: `HKLM\SYSTEM\...\Tcpip\Parameters\Domain` is set → PC is domain-joined
    2. At least one non-WireGuard LAN adapter is up with an IPv4 address
    3. `DsGetDcName` (dynamically loaded from `Netapi32.dll`, no new link dependency)
       with `DS_FORCE_REDISCOVERY | DS_RETURN_DNS_NAME | DS_IP_REQUIRED` finds a DC
    4. **Route check via `GetBestRoute`**: verifies that the route to the DC's IP address
       goes through a non-WireGuard adapter — prevents false positives in Homeoffice-VPN
       scenarios where the DC is reachable only through the WireGuard tunnel
  - Both stages exclude WireGuard virtual adapters (service-check + description-string)
  - New includes: `<lmcons.h>`, `<dsgetdc.h>` for `DOMAIN_CONTROLLER_INFOW` and `DS_*` constants
  - Detailed `LOG_DEBUG` output for every decision step (Stage1/Stage2/route/result)

- `WireGuardTray.cpp`: **Initial corporate-network check before NetWatch loop**
  - `bWasOnCorp` now initialized with a real `WGCPIsOnCorporateNetwork()` call at thread
    start instead of `false` — eliminates the race where a VM with an active tunnel at
    login triggered a handshake-timeout disconnect before the first corp-net check ran

- `WireGuardTray.cpp`: **Handshake-timeout suppressed on corporate network**
  - `nHandshakeFailCount` guarded by `&& !bOnCorp` — on corporate networks the VPN peer
    is intentionally not routable; the timeout must not fire in this situation
  - `nHandshakeFailCount` reset to 0 whenever `bOnCorp == true` so no accumulated count
    carries over when the machine leaves the corporate network

- `WireGuardTray.cpp`: **Corporate-network disconnect on every NetWatch tick**
  - Tunnel is disconnected whenever `bOnCorp && _bConnected`, not only on the `false→true`
    transition — correctly handles the case where the user manually connects while already
    on the corporate LAN
  - Balloon notification shown on every corporate-triggered disconnect
  - `false→true` transition balloon (without active tunnel) unchanged

- `helpers.h`: **`WGGetLastHandshakeSec` — "no handshake yet" returns 86400 instead of -1**
  - When all peers report timestamp `0` (tunnel up but never handshaked), the function
    now returns `86400` (24 h) so the handshake-timeout check can trigger a disconnect
  - This fixes the case where a broken tunnel in Homeoffice stayed up indefinitely
  - Log level for this condition changed from `LOG_WARN` to `LOG_DEBUG` to avoid log spam
    when the machine is on the corporate network (where no handshake is expected)

- `helpers.h`: **`WGGetLastHandshakeSec` — temp file name includes `GetTickCount()`**
  - File name changed from `wgcp_hs_<PID>.txt` to `wgcp_hs_<PID>_<Tick>.txt`
  - Prevents `ERROR_SHARING_VIOLATION` (err=32) when the Tooltip-refresh timer and the
    NetWatch thread call the function simultaneously within the same process
  - `CreateFileW` share mode changed from `0` to `FILE_SHARE_READ` for the write handle

- `helpers.h`: **`WGGetTrafficStats` — temp file name is now PID+Tick-unique**
  - `wgcp_stats.txt` → `wgcp_stats_<PID>_<Tick>.txt`, same motivation as handshake fix

- `installer/WireGuardCredentialProvider.nsi`: **`HandshakeTimeoutSec` default changed to 180**
  - Both the Erstinstall path and the migration/update path now write `"180"` instead of `"0"`
  - Update path uses `ReadRegStr` (not `ReadRegDWORD`) so already-migrated `REG_SZ` values
    are read correctly; overwrites `"0"` or empty on every install/update

- `installer/WireGuardCredentialProvider.nsi`: **Process termination via `nsProcess` plugin**
  - `WireGuardCPTray.exe` and `WireGuardShutdownService.exe` are now killed via
    `${nsProcess::KillProcess}` with an active poll loop (`${nsProcess::FindProcess}`,
    250 ms interval, 10 s timeout) that waits until the process is actually gone before
    NSIS copies new binaries — eliminates the "file in use" dialog during updates
  - `LogonUI.exe` is no longer killed (runs as SYSTEM/PPL; `taskkill /F` hung indefinitely;
    LogonUI reloads the CP DLL automatically on the next Winlogon cycle)

- `installer/Setup-YubiKey.ps1`: **Robust error handling for missing YubiKey / PIV driver**
  - New helper functions: `Invoke-Ykman`, `Get-YkmanVersion`, `Test-Prerequisites`,
    `Get-YubiKeyInfo`, `Test-PivSupport`, `Get-PivCertificate`
  - All `ykman` calls now go through `Invoke-Ykman` which captures stdout+stderr, checks
    `$LASTEXITCODE`, and prints ykman's own error message in a formatted block
  - `Get-YubiKeyInfo`: null-safe regex group access (was: `NullReferenceException` when no
    YubiKey found); categorised error messages with actionable hints (USB, driver, Minidriver
    download link)
  - `Test-PivSupport`: detects both missing PIV and disabled PIV (`PIV.*disabled`) with
    instructions to re-enable via `ykman config usb --enable PIV`
  - `Get-PivCertificate`: distinguishes empty slot, missing Minidriver, and corrupt
    certificate with specific messages and links
  - `Write-Fail` (which called `exit 1`) replaced with `return` — menu stays open after
    errors so the user can retry without restarting the script
  - Duplicate `Write-Step` in Option 2 removed

### Fixed

- `helpers.h`: `WGCPIsOnCorporateNetwork` — **false positive in Homeoffice-VPN scenario**
  - Stage 2 now performs a route check (`GetBestRoute`) after `DsGetDcName` succeeds.
    If the route to the DC's IP goes through a WireGuard adapter, the result is `false`
    (DC reachable only via VPN → not corporate LAN). Previously the function returned
    `true` in this case, causing the Tray to disconnect the tunnel immediately after
    connecting from the Homeoffice.

- `helpers.h`: `WGCPIsOnCorporateNetwork` — **`malloc(0)` undefined behaviour**
  - `GetAdaptersAddresses` sizing call result guarded with `if (ulSize > 0)` before
    `malloc`; on machines without adapters `ulSize` could be 0.

- `helpers.h`: `WGCPIsOnCorporateNetwork` — **`CoUninitialize` leak on error paths**
  - `bNeedCoUninit` flag now tracks the `CoInitializeEx` return value independently of
    later `HRESULT` values; all early-return paths call `CoUninitialize` correctly.

- `helpers.h`: `WGCPIsOnCorporateNetwork` — **Forward-reference compile error**
  - Function moved to after the `#define LOG_WARN / LOG_DEBUG / LOG_CRIT` macros;
    `WGGetConfigDir` emergency-fallback log call removed (forward-reference not resolvable
    there without restructuring).

- `helpers.h`: `WGGetLastHandshakeSec` — **`GetStdHandle(STD_ERROR_HANDLE)` returns NULL
  in LogonUI / service context**, causing `CreateProcess` to fail with
  `ERROR_INVALID_HANDLE` on some machines. stderr now redirected to `NUL` device.

- `helpers.h`: `WGGetLastHandshakeSec` — **only first peer parsed** (single `strchr`).
  All peer lines are now parsed; the minimum age (most recent handshake) is returned.

- `WireGuardTray.cpp`: **Tray UI — double-click triggered connect/disconnect twice**
  - Windows sends `WM_LBUTTONUP` before `WM_LBUTTONDBLCLK`; both were handled identically.
    Fixed via a `SetTimer(GetDoubleClickTime())` pattern: single-click arms a timer,
    double-click cancels the timer and executes once.

- `WireGuardTray.cpp`: **Profile "Activate" connected immediately instead of just selecting**
  - New `_SelectProfile(int)` method: changes `_nSelectedProfile` without calling `_Connect`.
    Shows a balloon "Profile X selected — click Connect to activate".

- `WireGuardTray.cpp`: **Delete enabled for actively selected (but disconnected) profile**
  - `MF_GRAYED` now set when `bIsSelected` (was: only when `bIsConnected`).

- `installer/WireGuardCredentialProvider.nsi`: **`taskkill.exe` without full path failed**
  in NSIS `ExecWait` context (no `PATH` lookup); replaced with `nsProcess` plugin calls.

- `installer/WireGuardCredentialProvider.nsi`: **`HandshakeTimeoutSec` not updated on
  existing installations** — `ReadRegDWORD` cannot read `REG_SZ` values written by the
  migration block; replaced with `ReadRegStr` + overwrite when value is `""` or `"0"`.

### Changed

- `WireGuardTray.cpp`: **Context menu restructured**
  - **Connect/Disconnect** action promoted to main menu (no submenu required for primary action)
  - Active profile name shown inline in the Connect label
  - 🟢 green dot prefix for the currently connected profile in the profile list
  - Submenu **Connect** grayed out when another profile's tunnel is active
  - **"No profile" tooltip** updated: "⚠ Kein Profil vorhanden / Klicken zum Importieren…"
  - **Disconnected tooltip** extended with "Klicken zum Verbinden / Click to connect" hint
  - YubiKey temp file for `ykman info` changed to `wgcp_yk_<PID>.txt` (PID-unique)

- `helpers.h`: `WGGetLastHandshakeSec` timeout raised from 3 s to 8 s

- `helpers.h`: Log level for "no handshake yet" changed `LOG_WARN` → `LOG_DEBUG`

---

## [2026.8.3] – 2026-08-04

### Added
- `WireGuardTray.cpp`: `_EncryptProfileViaMgr()` – after a profile import the
  WireGuardManager service is started temporarily so it converts the plain
  `.conf` to `.conf.dpapi`; the service is stopped and re-disabled immediately
  afterwards. A balloon notification informs the user while encryption is in
  progress. If the `.conf.dpapi` is not produced within 10 s a warning dialog
  is shown and the plain `.conf` is left in place for manual recovery.
- `WireGuardTray.cpp`: `_DeleteProfileAt(int)` – dedicated delete function that
  accepts a profile index. Blocks deletion only when the selected profile is
  actively connected (not globally when any tunnel is up). Includes elevated
  fallback via `ShellExecuteExW` with `runas` verb, adjusts `_nSelectedProfile`
  correctly when a preceding or the active profile is removed, and shows a
  balloon notification on success.
- `WireGuardTray.h`: declarations for `_EncryptProfileViaMgr` and
  `_DeleteProfileAt`; `IDM_PROFILE_DELETE_BASE 500` constant for the per-profile
  delete command range (500–563, parallel to `IDM_PROFILE_BASE` 300–363).

### Changed
- `WireGuardTray.cpp` – context menu restructured:
  - **Header** collapsed from two lines ("Verbunden" + active profile) into a
    single two-line block: line 1 = `🔒  WireGuard VPN` (app title), line 2 =
    `Status: Verbunden  |  Profil: <name>` (bilingual).
  - **Per-profile submenus**: each profile entry now expands into a submenu.
    Active + connected → `⏹ Trennen / Disconnect` + greyed-out delete.
    Active + disconnected or inactive → `▶ Aktivieren / Activate` + enabled
    delete (`🗑 Löschen / Delete`). Parent entry uses native `MF_CHECKED`
    checkmark for the active profile; no icon prefix in parent label.
  - Global top-level **Connect / Disconnect** entry removed (action moved into
    per-profile submenus).
  - Global top-level **Delete profile** entry removed (action moved into
    per-profile submenus as `IDM_PROFILE_DELETE_BASE + i`).
- `WireGuardTray.cpp`: `_DeleteProfile()` refactored into a thin legacy wrapper
  that delegates to `_DeleteProfileAt(_nSelectedProfile)`.
- `WireGuardTray.cpp`: `_ImportProfile()` calls `_EncryptProfileViaMgr()` after
  copying the `.conf`, deletes the plain file on success, and invokes
  `_LoadProfiles()` / `_RefreshStatus()` / `_UpdateTrayIcon()` only after
  encryption – ensuring the new profile is immediately visible in the menu.

### Fixed
- Profile import: imported `.conf` files were not encrypted because
  WireGuardManager was disabled; plain `.conf` remained unreadable by the
  Credential Provider.
- Profile import: tray menu was refreshed before the `.conf.dpapi` existed,
  so newly imported profiles did not appear until the next tray restart.
- Profile delete: deletion was blocked for **all** profiles whenever any tunnel
  was connected; corrected to block only the profile that is actively connected.

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
- PIN field in pre-logon tile: shown when disconnected, hidden when connected
- `GetSerialization` triggers `CommandLinkClicked` when user presses Enter
- SC-Watch thread: detects card present at startup, updates status on every tick
- Log encoding changed from UTF-16LE to UTF-8 (no BOM, readable with standard tools)
- Default log level changed back to CRIT (1) – DEBUG only when explicitly configured
- Version bumped to 2026.7.31 in all RC files

### Fixed
- Pre-logon PIN was never received (`CPFT_PASSWORD_TEXT` does not call `SetStringValue`)
- YubiKey RSA2048 certificate truncated at 258 bytes → fixed via GET RESPONSE chaining
- `SHCreateDirectoryExW` failure silently prevented all CP logging as SYSTEM
- CP and Tray wrote to same log file in `C:\Windows\Temp`
- `CERT_STORE_PROV_SMARTCARD` not available in modern SDK → replaced with direct PIV GET DATA APDU
- `SCardFreeMemory(nullptr)` compilation error → removed unnecessary reader status call
- Leftover `CredLog` call causing compilation error after debug cleanup

---

## [2026.7.30] – 2026-07-30

### Added
- **Post-Logon Tray Application** (`WireGuardCPTray.exe`)
- **Installer overhaul** (`WireGuardCredentialProvider.nsi`)
- **Tray icons**: custom ICO files with alpha channel and multiple sizes
- **Application icon** (`icon.ico`) embedded in `WireGuardCPTray.exe`
- **`resource.h`**: `IDI_TRAY_CONNECTED (103)` and `IDI_TRAY_DISCONNECTED (104)`

### Changed
- `helpers.h`: `WGCP_TRAY_BUILD` preprocessor guard
- `helpers.h`: `WGGetConfigDir()` replaces hardcoded `WG_CONFIG_DIR` define
- `helpers.h`: `WGEnumProfiles()` uses `*.dpapi` search pattern
- All source code comments converted to English throughout

### Fixed
- Various tray, installer and resource fixes (see full entry above)

---

## [2026.7.6] – 2026-07-29

### Changed
- Registry key moved to `HKEY_LOCAL_MACHINE\SOFTWARE\Jens Kaesler\WireGuard Credential Provider`
- All source code comments, log messages, and UI strings fully translated to English

### Fixed
- `min()` call in smartcard PIN copy replaced with explicit ternary
- Duplicate `SetStringValue` stub removed

---

## [2026.7.14] – 2026-07-25

### Added
- Comprehensive debug logging across all smartcard functions

---

## [2026.7.13] – 2026-07-25

### Added
- **Smartcard / YubiKey PIV authentication** – optional second factor before connecting

---

## [2026.7.12] – 2026-07-24

### Changed
- General code cleanup and refactoring ahead of smartcard feature

---

## [2026.7.11] – 2026-07-24

### Fixed
- Installer: robocopy, WOW64 redirection, LogonUI kill, System32 path fixes

---

## [2026.7.5] – 2026-07-24

### Added
- Log rotation, daily log files, `InstallDir` registry base path

---

## [2026.7.4] – 2026-07-23

### Changed
- `FieldDescriptors.h` extracted; `volatile` fields; shutdown service fixes

---

## [2026.7.3] – 2026-07-23

### Added
- **WireGuardShutdownService**: `SERVICE_CONTROL_PRESHUTDOWN` handler

---

## [2026.7.2] – 2026-07-23

### Added
- Auto-refresh, connection timer, traffic statistics, color-coded icons

---

## [2026.7.1] – 2026-07-23

### Added
- Profile dropdown, connect/disconnect button, status display, service detection

---

## [2026.7.0] – 2026-07-23

### Added
- Initial release: WireGuard Credential Provider DLL (`ICredentialProvider`)
