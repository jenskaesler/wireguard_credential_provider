# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning follows the scheme `<Year>.<Month>.<Release>`.

---

## [2026.11.4] - 2026-11-22

### Changed

- **`bump-version.ps1` commits, tags and pushes** — After updating the version files the
  script commits them (`chore: bump version to X`), creates the tag `vX` on that commit
  and pushes commit and tag to `origin`. It aborts before changing any file if the tag
  already exists. This ensures the release tag always contains the new version number.

### Fixed

- **Check for Updates gave no feedback** — A manual check (About dialog button or tray
  menu) now always reports its result: the update popup if a newer version exists,
  otherwise a message box saying the installed version is up to date or that GitHub
  could not be reached. The About button returns from *Checking…* to *Check for Updates*
  when the check finishes.

- **Manual update check did nothing after the startup check** — Once the automatic
  startup check had finished, its thread handle was never released, so every later
  manual check only signalled an event nobody was waiting on. A finished thread is now
  cleaned up and a new check is started.

---

## [2026.11.3] - 2026-11-22

### Fixed

- **Installer: shutdown service was never stopped during install / upgrade** — The
  `SimpleSC` plugin in the NSIS `x86-unicode` folder was the ANSI build, so the
  service name reached the plugin garbled and `StopService` failed silently. Killing
  the process via `nsProcess` does not work for a SYSTEM service either, so
  `WireGuardShutdownService.exe` in System32 stayed locked and robocopy retried
  forever (error 32, 30-second waits). The installer now stops the service with the
  built-in `net stop`, which waits until the service has stopped; the exit code is
  written to the install log.

- **Installer: robocopy no longer hangs on a locked file** — Both robocopy calls use
  `/R:5 /W:2` instead of the default of one million retries.

- **Installer: German umlauts** — German translations now use proper umlauts
  (*Lädt*, *Benötigt*, *Möchten*, *durchführen*, *öffnen*, *Prüfe*) instead of
  transcriptions; the script is saved as UTF-8 with BOM.

### Removed

- **`SimpleSC` NSIS plugin dependency** — No longer required to build the installer.

---

## [2026.11.2] - 2026-11-22

### Changed

- **Update notification: persistent popup instead of balloon tip** — The Windows balloon
  notification (auto-dismissed after 15 s) has been replaced by a custom popup window
  that stays visible until the user acts. Clicking the popup opens the GitHub release
  page; the X button closes it without opening anything.

- **Update popup: dark / light mode** — The popup follows the Windows app theme
  (`AppsUseLightTheme`) and reacts live to theme changes.

### Fixed

- **About dialog: Check for Updates button had no effect** — The button posted
  `IDM_CHECK_NOW` to the main window, which is disabled while the modal About dialog is
  open, so the message was not processed. The dialog now starts the update check
  directly.

- **About dialog: emoji in Changelog / Releases links** — The legacy Win32 SysLink
  control does not render emoji; the 📋 / 🚀 prefixes have been removed from the labels.

### Documentation

- **`docs/PRIVACY.md`** — Updated for the configurable startup delay
  (`AutoUpdateCheckInterval`, default 5 minutes), the manual *Check for Updates* button,
  the new popup notification, and the correct silent-install parameter `/UPDATECHECK=0`
  (previously documented as the non-existent `/NOUPDATECHECK`).

---

## [2026.11.1] - 2026-11-21

### Added

- **About dialog: Changelog & Releases links** — The *About* dialog now shows two
  SysLink hyperlinks directly below the GitHub link: 📋 Changelog (opens
  `CHANGELOG.md` on GitHub) and 🚀 Releases (opens the GitHub Releases page).
  Both link labels are bilingual (German / English via the existing `T()` macro).

- **About dialog: Check for Updates button** — A *Check for Updates* / *Auf Updates
  prüfen* button triggers an immediate GitHub release check without waiting for the
  configurable startup delay. The button disables itself with *Checking…* / *Prüfung
  läuft…* feedback while the check is in progress.

- **`AutoUpdateCheckVisible` registry key** (REG_DWORD, default `1`) — When set to
  `0`, hides the update-check toggle from the tray context menu and the *Check for
  Updates* button from the About dialog. Intended for managed deployments where
  administrators want to suppress update-check UI entirely. The OK button is centred
  automatically when the button is hidden.

- **`AutoUpdateCheckInterval` registry key** (REG_DWORD, default `5`) — Configures
  the startup delay in minutes before the first automatic update check. Replaces the
  previously hard-coded 10-minute wait. Setting the value to `0` triggers the check
  immediately at startup.

- **Installer: `/UPDATECHECKVISIBLE=n` silent parameter** — Sets
  `AutoUpdateCheckVisible` during silent install; default written on first install is
  `1`.

- **Installer: `/UPDATECHECKINTERVAL=n` silent parameter** — Sets
  `AutoUpdateCheckInterval` during silent install; default written on first install is
  `5`.

---

## [2026.11.0] - 2026-11-21

### Added

- **📄 `docs/SECURITY.md`** — Security policy with vulnerability reporting channels
  (GitHub Private Vulnerability Reporting, e-mail), response timeline, in-scope / out-of-scope
  components, and a note on the named-pipe security boundary.

- **🔒 `docs/PRIVACY.md`** — Privacy statement. Documents the only outbound network
  request (optional GitHub update check: IP address + User-Agent sent to `api.github.com`).
  Covers log files (local only), YubiKey serial number (in-memory cache, never written to
  disk or transmitted), and VPN traffic (not inspected by this application).

- **📋 `docs/THIRD-PARTY-NOTICES.md`** — License notices for third-party components
  installed by the NSIS installer: WireGuard for Windows (MIT, WireGuard LLC) and
  YubiKey Manager CLI (Apache 2.0, Yubico AB).

- **`bump-version.ps1`** — Interactive version-bump script (format `YYYY.MM.N`). Updates
  `FILEVERSION` / `ProductVersion` in both `.rc` resource files and the README version
  badge. Modes: interactive menu, or non-interactive with `-Patch` / `-Month` / `-Version`.
  Integrated into `build.bat` as step 0; exits with code 10 on a bump to remind the user
  to rebuild the solution in Visual Studio before continuing.

- **`installer/build.bat`: LICENSE pulled from repo root** — The script now copies
  `LICENSE` from the repository root to `installer/content/docs/LICENSE.txt` before
  compiling the NSIS installer. The file is generated at build time and no longer checked
  into version control.

- **`installer/build.bat`: SHA-256 checksum file** — After a successful build, the script
  generates `installer/checksums.txt` in the standard `sha256sum` format
  (`<hash>  <filename>`) using PowerShell's `Get-FileHash`. The file is ready to upload
  alongside the installer on GitHub Releases.

### Changed

- **`installer/WireGuardCredentialProvider.nsi`**: NSIS license page updated from
  `content\docs\LICENSE.rtf` to `content\docs\LICENSE.txt` (the RTF file was equivalent
  plain text; NSIS renders both identically).

- **`installer/content/docs/`** removed from version control. The folder is now generated
  by `build.bat` from the root `LICENSE` and is listed in `.gitignore`.

- **`README.md`**: License badge corrected from "MIT" to "Non-Commercial Copyleft" to
  accurately reflect the license in `LICENSE`. Added a **Security & Privacy** section
  linking to the three new documentation files.

- **`.gitignore`**: Replaced global `docs/` ignore (which would have hidden the new
  `docs/` directory) with a scoped `installer/content/docs/` ignore. Added
  `installer/checksums.txt`.

- **`installer/README.md`** and **`installer/build.bat`** translated to English.

---

## [2026.9.9] - 2026-09-10

### Added

- **✏️ Profile Editor: Edit** — WireGuard profiles can now be edited directly from the
  tray. New menu item `✏ Edit...` in the profile submenu:
  - Decrypts `.conf.dpapi` via the SYSTEM helper service (Named Pipe, Op 3)
  - Opens the configuration in an embedded text editor dialog (monospace,
    scrollable, with syntax highlighting for keys/values)
  - Save: writes plaintext `.conf` → deletes old `.conf.dpapi` → briefly starts
    the `WireGuardManager` service (it encrypts and cleans up `.conf`) —
    identical to the import flow, guarantees WireGuard-compatible encryption
  - If the profile was connected: disconnects before editing, offers reconnection
    after saving

- **💾 Profile Export** — New menu item `💾 Export...` in the profile submenu:
  - Decrypts `.conf.dpapi` via the SYSTEM helper service (Named Pipe, Op 3)
  - Opens a Save dialog with the profile name pre-filled
  - Writes the decrypted configuration as plaintext `.conf` to the chosen path

- **🔧 Named Pipe IPC (WireGuardCPHelperSvc)** — The `WireGuardShutdownHelper` service
  (SYSTEM context) exposes a Named Pipe `\\.\pipe\WireGuardCPHelperSvc` through which
  the tray (Medium Integrity) gains privileged access to the WireGuard configuration
  folder (ACL: SYSTEM-only):
  - Op 1 `READ` - read raw bytes
  - Op 2 `WRITE` - write raw bytes
  - Op 3 `READ_DECRYPTED` - read + DPAPI decryption (SYSTEM context)
  - Op 4 `WRITE_ENCRYPTED` - DPAPI encryption + atomic write
  - Op 5 `WRITE_CONF` - write plaintext `.conf`
  - Op 6 `DELETE_FILE` - delete file (`.conf` or `.conf.dpapi`)

### Fixed

- **About dialog: GitHub link not visible** — `WC_LINK` (SysLink control) requires
  `comctl32.dll` v6. The custom manifest (`Microsoft.Windows.Common-Controls 6.0.0.0`)
  was never embedded because `<ManifestFile>` in the `<Link>` block only sets the
  output path, not an additional manifest file. Fix: `<AdditionalManifestFiles>` in
  the `<Manifest>` block + `/MANIFESTUAC:NO` to avoid merge conflicts.

- **About dialog: Link icon rendered as box** — The emoji 🔗 (U+1F517, surrogate pair)
  is not rendered by `WC_LINK`. Replaced with `⧉` (U+29C9, BMP).

- **Context menu white in dark mode** — `SetWindowTheme` on the tray window (`_hWnd`)
  has no effect on popup menu HWNDs (standalone top-level windows, class `#32768`).
  Fix: `AllowDarkModeForWindow(_hWnd, TRUE)` (uxtheme ordinal 133) after window
  creation; `SetPreferredAppMode` + `RefreshImmersiveColorPolicyState` (ordinal 104)
  at startup; `WH_CALLWNDPROC` hook that calls `SetWindowTheme(DarkMode_Explorer)`
  directly on `WM_CREATE` of each menu window.

---

## [2026.9.7] - 2026-09-10

### Fixed

- **Profile editor: DPAPI flags corrected** — WireGuard encrypts `.conf.dpapi` with
  `CRYPTPROTECT_LOCAL_MACHINE | CRYPTPROTECT_UI_FORBIDDEN`. The flag
  `CRYPTPROTECT_LOCAL_MACHINE` is invalid for `CryptUnprotectData` (MSDN only permits
  `CRYPTPROTECT_UI_FORBIDDEN` there) — this caused the original `NTE_BAD_FLAGS` error
  `0x8009000B`. The erroneous SYSTEM impersonation was removed. Decryption:
  `CryptUnprotectData(CRYPTPROTECT_UI_FORBIDDEN)`, no impersonation required
  (LOCAL_MACHINE blobs are machine-wide admin resources). Encryption:
  `CryptProtectData(CRYPTPROTECT_LOCAL_MACHINE | CRYPTPROTECT_UI_FORBIDDEN)` so that
  WireGuard can read the file again.

---

## [2026.9.6] - 2026-09-10

### Fixed

- **Profile editor: DPAPI decryption under SYSTEM context** — WireGuard encrypts
  `.conf.dpapi` files using the DPAPI user context of SYSTEM (no
  `CRYPTPROTECT_LOCAL_MACHINE` flag). The tray app ran as Administrator and therefore
  could not decrypt (error `0x8009000B`). Fix: token impersonation via the
  `winlogon.exe` process token (`DuplicateTokenEx` + `SetThreadToken`) before each
  `CryptUnprotectData`/`CryptProtectData` call, followed by `RevertToSelf()`.

---

## [2026.9.5] - 2026-09-10

### Fixed

- **Systray: About dialog layout and transparency** — Background of static controls
  made transparent (`WM_CTLCOLORSTATIC` + `TRANSPARENT` bkmode), GitHub SysLink now
  visible (`ICC_LINK_CLASS` in `InitCommonControlsEx`, `LWS_NOPREFIX` flag), layout
  revised (version + copyright on separate lines, correct y-positions).

---

## [2026.9.4] - 2026-09-10

### Changed

- **Systray: About dialog redesigned** — Replaces two `MessageBox` calls with a custom
  window featuring a dark-blue header (`#1A376E`, Segoe UI Bold 13pt), info icon,
  version/copyright line, and a clickable GitHub link (`WC_LINK` SysLink control).
  The version is now read from the EXE FileVersion resource when no installer registry
  entry is present (no more *(unknown)* in dev builds).

---

## [2026.9.3] - 2026-09-09

### Added

- **Systray: Profile editor** — WireGuard profiles can now be edited directly from the
  tray. Each profile submenu contains a new entry `✏ Edit...`. On click, the encrypted
  `.conf.dpapi` file is decrypted via Windows DPAPI (`CRYPTPROTECT_LOCAL_MACHINE`) and
  displayed in the embedded text editor (monospace, scrollable). After saving, the file
  is atomically re-encrypted (temp → `MoveFileEx`). If the profile was connected, it
  is disconnected before editing and reconnection is offered after saving.

---

## [2026.9.2] - 2026-09-09

### Fixed

- **Systray: VPN status detection after lock-screen connection** — `_RefreshStatus()`
  only checked the currently selected profile. If the Credential Provider had started a
  tunnel from the lock screen and that profile did not match the computer name, the tray
  icon remained red. Fix: after a negative result, all profiles are now checked; the
  first active profile is automatically adopted as the selected profile.

---

## [2026.9.1] - 2026-09-09

### Changed

- **Systray: About dialog simplified** — Complex Win32 dialog replaced by two standard
  `MessageBox` windows: an info box with version, copyright, description, and GitHub URL
  as text, followed by a prompt "Open GitHub repository in browser?"

### Fixed

- **Systray: Menu icon spacing unified** — All entries now consistently use
  `Icon + 2 spaces + text`; `❌ Exit` and `🔑 Open Yubico Authenticator…` had
  inconsistent spacing.
- **Constructor: member initialization** — `_bAutoUpdateCheck`, `_bUpdateBalloonActive`,
  `_hUpdateThread`, `_hUpdateStop` were erroneously initialized inside the
  YubiKey-not-detected branch of `_ShowContextMenu` instead of the constructor
  (caused thread leak and reset update status on every menu open without YubiKey).

---

## [2026.8.6] - 2026-09-09

### Added

- **Systray: Automatic update check** (`🔄 Check for updates` in the context menu)
  - New toggle menu item with checkmark — enables/disables automatic checking for new
    GitHub releases
  - State stored in `HKLM\SOFTWARE\Jens Kaesler\WireGuard Credential Provider\AutoUpdateCheck`
    (DWORD) — can be controlled by administrators via registry or GPO (default: 1 = enabled)
  - 10 minutes after tray start,
    `api.github.com/repos/jenskaesler/wireguard_credential_provider/releases/latest`
    is queried via WinHTTP (HTTPS)
  - If a newer version is available, a balloon tip appears:
    *"🔄 Update available: x.x.x — Click here to download"*
  - Clicking the balloon opens the GitHub release page directly in the browser
  - Version is compared against the installed version from the Windows uninstall key
    (`DisplayVersion`); `v` prefix in the GitHub tag name is tolerated
  - Thread stops immediately when the setting is disabled or the app exits

---

## [2026.8.5] - 2026-09-09

### Added

- **Systray: About dialog** (`ℹ️ Information...` in the context menu)
  - New menu option before "Exit" opens a modal dialog with app name, version (from
    the Windows uninstall registry key `DisplayVersion`), copyright notice, and a short
    description
  - **GitHub link** as an owner-drawn button (blue, underlined) opens
    `https://github.com/jenskaesler/wireguard_credential_provider` directly in the
    browser
  - New constants in `helpers.h`: `WGCP_REG_UNINSTALL`, `WGCP_GITHUB_URL`,
    `WGCP_VERSION_FALLBACK`
  - Implementation analogous to the existing PIN dialog (programmatic Win32 dialog,
    no `.rc` template dependency)

- **Systray: One-step profile switch** (`⇄ Switch to <profile>`)
  - New method `_SwitchProfile(int profileIndex)` — disconnects the active profile and
    connects the selected one in a single action
  - Appears in the profile submenu when a different profile is already connected
  - New menu ID range `IDM_PROFILE_SWITCH_BASE 800`

- **Systray: YubiKey serial cached asynchronously**
  - Serial number is retrieved in `_SmartcardWatchThread` when the card is inserted via
    `ykman` and stored in `_wszYkSerial`
  - Menu construction no longer blocks (no synchronous `WaitForSingleObject` when
    opening the context menu)

- **Systray: Open configuration folder** — Missing menu item added
  (`📁 Open configuration folder...`); `_OpenConfigDir()` and `IDM_OPEN_CONFIG_DIR`
  were already implemented but never added to the menu.

### Fixed

- **Systray: Profile switch was ignored** — `MF_POPUP` parent entries send no
  `WM_COMMAND`; the previous `IDM_PROFILE_BASE+i` handler was dead code. Each
  "Connect" option in the submenu now has a unique ID from the range
  `IDM_PROFILE_CONNECT_BASE+i`.

- **Systray: Delete was too restrictive** — "Delete" was grayed out for
  selected-but-not-connected profiles. Now: grayed out only when the profile is
  actively connected.

---

## [2026.8.4] - 2026-08-10

### Added

- `helpers.h`: **Two-stage corporate network detection** (`WGCPIsOnCorporateNetwork`)
  - Stage 1 (unchanged): NLA `DOMAIN_AUTHENTICATED` via `INetworkListManager`
  - Stage 2 (new fallback): detects domain-joined VMs and machines where NLA reports
    `PRIVATE` instead of `DOMAIN_AUTHENTICATED` (observed on Red Hat VirtIO / Hyper-V
    adapters because the DC was not reachable when NLA first classified the network at
    boot):
    1. Registry check: `HKLM\SYSTEM\...\Tcpip\Parameters\Domain` is set → PC is
       domain-joined
    2. At least one non-WireGuard LAN adapter is up with an IPv4 address
    3. `DsGetDcName` (dynamically loaded from `Netapi32.dll`, no new link dependency)
       with `DS_FORCE_REDISCOVERY | DS_RETURN_DNS_NAME | DS_IP_REQUIRED` finds a DC
    4. **Route check via `GetBestRoute`**: verifies that the route to the DC's IP
       address goes through a non-WireGuard adapter — prevents false positives in
       home-office VPN scenarios where the DC is reachable only through the WireGuard
       tunnel
  - Both stages exclude WireGuard virtual adapters (service-check + description string)
  - New includes: `<lmcons.h>`, `<dsgetdc.h>` for `DOMAIN_CONTROLLER_INFOW` and
    `DS_*` constants
  - Detailed `LOG_DEBUG` output for every decision step (Stage1/Stage2/route/result)

- `WireGuardTray.cpp`: **Initial corporate-network check before NetWatch loop**
  - `bWasOnCorp` now initialized with a real `WGCPIsOnCorporateNetwork()` call at
    thread start instead of `false` — eliminates the race where a VM with an active
    tunnel at login triggered a handshake-timeout disconnect before the first corp-net
    check ran

- `WireGuardTray.cpp`: **Handshake timeout suppressed on corporate network**
  - `nHandshakeFailCount` guarded by `&& !bOnCorp` — on corporate networks the VPN
    peer is intentionally not routable; the timeout must not fire in this situation
  - `nHandshakeFailCount` reset to 0 whenever `bOnCorp == true` so no accumulated
    count carries over when the machine leaves the corporate network

- `WireGuardTray.cpp`: **Corporate-network disconnect on every NetWatch tick**
  - Tunnel is disconnected whenever `bOnCorp && _bConnected`, not only on the
    `false -> true` transition — correctly handles the case where the user manually
    connects while already on the corporate LAN
  - Balloon notification shown on every corporate-triggered disconnect
  - `false -> true` transition balloon (without active tunnel) unchanged

- `helpers.h`: **`WGGetLastHandshakeSec` — "no handshake yet" returns 86400 instead
  of -1**
  - When all peers report timestamp `0` (tunnel up but never handshaked), the function
    now returns `86400` (24 h) so the handshake-timeout check can trigger a disconnect
  - This fixes the case where a broken tunnel at home stayed up indefinitely
  - Log level for this condition changed from `LOG_WARN` to `LOG_DEBUG` to avoid log
    spam when the machine is on the corporate network (where no handshake is expected)

- `helpers.h`: **`WGGetLastHandshakeSec` — temp file name includes `GetTickCount()`**
  - File name changed from `wgcp_hs_<PID>.txt` to `wgcp_hs_<PID>_<Tick>.txt`
  - Prevents `ERROR_SHARING_VIOLATION` (err=32) when the tooltip-refresh timer and the
    NetWatch thread call the function simultaneously within the same process
  - `CreateFileW` share mode changed from `0` to `FILE_SHARE_READ` for the write handle

- `helpers.h`: **`WGGetTrafficStats` — temp file name is now PID+Tick-unique**
  - `wgcp_stats.txt` -> `wgcp_stats_<PID>_<Tick>.txt`, same motivation as handshake fix

- `installer/WireGuardCredentialProvider.nsi`: **`HandshakeTimeoutSec` default changed
  to 180**
  - Both the initial install path and the migration/update path now write `"180"`
    instead of `"0"`
  - Update path uses `ReadRegStr` (not `ReadRegDWORD`) so already-migrated `REG_SZ`
    values are read correctly; overwrites `"0"` or empty on every install/update

- `installer/WireGuardCredentialProvider.nsi`: **Process termination via `nsProcess`
  plugin**
  - `WireGuardCPTray.exe` and `WireGuardShutdownService.exe` are now killed via
    `${nsProcess::KillProcess}` with an active poll loop (`${nsProcess::FindProcess}`,
    250 ms interval, 10 s timeout) that waits until the process is actually gone before
    NSIS copies new binaries — eliminates the "file in use" dialog during updates
  - `LogonUI.exe` is no longer killed (runs as SYSTEM/PPL; `taskkill /F` hung
    indefinitely; LogonUI reloads the CP DLL automatically on the next Winlogon cycle)

- `installer/Setup-YubiKey.ps1`: **Robust error handling for missing YubiKey / PIV
  driver**
  - New helper functions: `Invoke-Ykman`, `Get-YkmanVersion`, `Test-Prerequisites`,
    `Get-YubiKeyInfo`, `Test-PivSupport`, `Get-PivCertificate`
  - All `ykman` calls now go through `Invoke-Ykman` which captures stdout+stderr,
    checks `$LASTEXITCODE`, and prints ykman's own error message in a formatted block
  - `Get-YubiKeyInfo`: null-safe regex group access (was: `NullReferenceException` when
    no YubiKey found); categorized error messages with actionable hints (USB, driver,
    Minidriver download link)
  - `Test-PivSupport`: detects both missing PIV and disabled PIV (`PIV.*disabled`) with
    instructions to re-enable via `ykman config usb --enable PIV`
  - `Get-PivCertificate`: distinguishes empty slot, missing Minidriver, and corrupt
    certificate with specific messages and links
  - `Write-Fail` (which called `exit 1`) replaced with `return` — menu stays open after
    errors so the user can retry without restarting the script
  - Duplicate `Write-Step` in Option 2 removed

### Fixed

- `helpers.h`: `WGCPIsOnCorporateNetwork` — **false positive in home-office VPN
  scenario**
  - Stage 2 now performs a route check (`GetBestRoute`) after `DsGetDcName` succeeds.
    If the route to the DC's IP goes through a WireGuard adapter, the result is `false`
    (DC reachable only via VPN → not corporate LAN). Previously the function returned
    `true` in this case, causing the tray to disconnect the tunnel immediately after
    connecting from home.

- `helpers.h`: `WGCPIsOnCorporateNetwork` — **`malloc(0)` undefined behaviour**
  - `GetAdaptersAddresses` sizing call result guarded with `if (ulSize > 0)` before
    `malloc`; on machines without adapters `ulSize` could be 0.

- `helpers.h`: `WGCPIsOnCorporateNetwork` — **`CoUninitialize` leak on error paths**
  - `bNeedCoUninit` flag now tracks the `CoInitializeEx` return value independently of
    later `HRESULT` values; all early-return paths call `CoUninitialize` correctly.

- `helpers.h`: `WGCPIsOnCorporateNetwork` — **forward-reference compile error**
  - Function moved to after the `#define LOG_WARN / LOG_DEBUG / LOG_CRIT` macros;
    `WGGetConfigDir` emergency-fallback log call removed (forward-reference not
    resolvable there without restructuring).

- `helpers.h`: `WGGetLastHandshakeSec` — **`GetStdHandle(STD_ERROR_HANDLE)` returns
  NULL in LogonUI / service context**, causing `CreateProcess` to fail with
  `ERROR_INVALID_HANDLE` on some machines. stderr now redirected to `NUL` device.

- `helpers.h`: `WGGetLastHandshakeSec` — **only first peer parsed** (single `strchr`).
  All peer lines are now parsed; the minimum age (most recent handshake) is returned.

- `WireGuardTray.cpp`: **Tray UI — double-click triggered connect/disconnect twice**
  - Windows sends `WM_LBUTTONUP` before `WM_LBUTTONDBLCLK`; both were handled
    identically. Fixed via a `SetTimer(GetDoubleClickTime())` pattern: single-click
    arms a timer, double-click cancels the timer and executes once.

- `WireGuardTray.cpp`: **Profile "Activate" connected immediately instead of just
  selecting**
  - New `_SelectProfile(int)` method: changes `_nSelectedProfile` without calling
    `_Connect`. Shows a balloon "Profile X selected — click Connect to activate".

- `WireGuardTray.cpp`: **Delete enabled for actively selected (but disconnected)
  profile**
  - `MF_GRAYED` now set when `bIsSelected` (was: only when `bIsConnected`).

- `installer/WireGuardCredentialProvider.nsi`: **`taskkill.exe` without full path
  failed** in NSIS `ExecWait` context (no `PATH` lookup); replaced with `nsProcess`
  plugin calls.

- `installer/WireGuardCredentialProvider.nsi`: **`HandshakeTimeoutSec` not updated on
  existing installations** — `ReadRegDWORD` cannot read `REG_SZ` values written by the
  migration block; replaced with `ReadRegStr` + overwrite when value is `""` or `"0"`.

### Changed

- `WireGuardTray.cpp`: **Context menu restructured**
  - **Connect/Disconnect** action promoted to main menu (no submenu required for
    primary action)
  - Active profile name shown inline in the Connect label
  - 🟢 green dot prefix for the currently connected profile in the profile list
  - Submenu **Connect** grayed out when another profile's tunnel is active
  - **"No profile" tooltip** updated: `"⚠ No profile found / Click to import..."`
  - **Disconnected tooltip** extended with `"Click to connect"` hint
  - YubiKey temp file for `ykman info` changed to `wgcp_yk_<PID>.txt` (PID-unique)

- `helpers.h`: `WGGetLastHandshakeSec` timeout raised from 3 s to 8 s

- `helpers.h`: Log level for "no handshake yet" changed `LOG_WARN` -> `LOG_DEBUG`

---

## [2026.8.3] - 2026-08-04

### Added

- `WireGuardTray.cpp`: `_EncryptProfileViaMgr()` — after a profile import the
  WireGuardManager service is started temporarily so it converts the plain `.conf` to
  `.conf.dpapi`; the service is stopped and re-disabled immediately afterwards. A
  balloon notification informs the user while encryption is in progress. If the
  `.conf.dpapi` is not produced within 10 s a warning dialog is shown and the plain
  `.conf` is left in place for manual recovery.
- `WireGuardTray.cpp`: `_DeleteProfileAt(int)` — dedicated delete function that accepts
  a profile index. Blocks deletion only when the selected profile is actively connected
  (not globally when any tunnel is up). Includes elevated fallback via `ShellExecuteExW`
  with `runas` verb, adjusts `_nSelectedProfile` correctly when a preceding or the
  active profile is removed, and shows a balloon notification on success.
- `WireGuardTray.h`: declarations for `_EncryptProfileViaMgr` and `_DeleteProfileAt`;
  `IDM_PROFILE_DELETE_BASE 500` constant for the per-profile delete command range
  (500-563, parallel to `IDM_PROFILE_BASE` 300-363).

### Changed

- `WireGuardTray.cpp` — context menu restructured:
  - **Header** collapsed from two lines ("Connected" + active profile) into a single
    two-line block: line 1 = `🔒  WireGuard VPN` (app title), line 2 =
    `Status: Connected  |  Profile: <name>`.
  - **Per-profile submenus**: each profile entry now expands into a submenu.
    Active + connected → `⏹ Disconnect` + greyed-out delete.
    Active + disconnected or inactive → `▶ Activate` + enabled delete
    (`🗑 Delete`). Parent entry uses native `MF_CHECKED` checkmark for the active
    profile; no icon prefix in parent label.
  - Global top-level **Connect / Disconnect** entry removed (action moved into
    per-profile submenus).
  - Global top-level **Delete profile** entry removed (action moved into per-profile
    submenus as `IDM_PROFILE_DELETE_BASE + i`).
- `WireGuardTray.cpp`: `_DeleteProfile()` refactored into a thin legacy wrapper that
  delegates to `_DeleteProfileAt(_nSelectedProfile)`.
- `WireGuardTray.cpp`: `_ImportProfile()` calls `_EncryptProfileViaMgr()` after copying
  the `.conf`, deletes the plain file on success, and invokes `_LoadProfiles()` /
  `_RefreshStatus()` / `_UpdateTrayIcon()` only after encryption — ensuring the new
  profile is immediately visible in the menu.

### Fixed

- Profile import: imported `.conf` files were not encrypted because WireGuardManager
  was disabled; plain `.conf` remained unreadable by the Credential Provider.
- Profile import: tray menu was refreshed before the `.conf.dpapi` existed, so newly
  imported profiles did not appear until the next tray restart.
- Profile delete: deletion was blocked for **all** profiles whenever any tunnel was
  connected; corrected to block only the profile that is actively connected.

---

## [2026.8.2] - 2026-08-03

### Fixed

- `dll.cpp`: Removed debug load marker writing to `C:\Windows\Temp`
  (DllMain + DllGetClassObject)
- `WireGuardProvider.cpp`: Removed `ProvLog` helper and all call sites writing to
  `C:\Windows\Temp`
- `helpers.h`: Removed all fallback log paths to `C:\Windows\Temp` — if INSTDIR log
  dir is unavailable, no log is written
- `helpers.h`: `ReadRegDword` now also accepts `REG_SZ` with decimal string
  (e.g. `"180"`) — no hex conversion needed in Registry Editor
- `WireGuardTray.cpp`: `WireGuardManager` service disabled on tray init to prevent
  `wireguard.exe` UI respawn
- `WireGuardTray.cpp`: Send `WM_CLOSE` before `TerminateProcess` so `wireguard.exe`
  can clean up its tray icon via `Shell_NotifyIcon(NIM_DELETE)`
- `installer/WireGuardCredentialProvider.nsi`: Migrate legacy `REG_DWORD` values
  (`LogLevel`, `LogRetentionDays`, `HandshakeTimeoutSec`) to `REG_SZ` on update
- `installer/WireGuardCredentialProvider.nsi`: `HKLM\Run` autostart replaced by
  `CommonStartup` shortcut with `RunAsAdministrator` flag — tray now starts as admin
  on login
- Silent install parameters added: `/LOGLEVEL`, `/HANDSHAKE`, `/THUMBPRINT`,
  `/SMARTCARD`, `/PINREQUIRED`, `/DISCONNECTREMOVE`, `/TILELABEL`, `/CONFIGDIR`
- All registry keys now written directly by installer (`configure.reg` removed)

---

## [2026.8.1] - 2026-08-01

### Added

- **Handshake timeout** — auto-disconnect when WireGuard handshake is too old
  - New registry key `HandshakeTimeoutSec` (REG_DWORD, 0 = disabled)
  - Pre-logon: checked every second in SC-Watch thread
  - Post-logon: checked every 10 seconds in Network-Watch thread
  - `LOG_CRIT` entry when disconnect is triggered (visible at LogLevel=1)
  - Balloon notification in tray on disconnect
  - `WGGetLastHandshakeSec()` in `helpers.h`: parses
    `wg show <profile> latest-handshakes`

- **Corporate network detection** — auto-disconnect when on domain network
  - `WGCPIsOnCorporateNetwork()` via `INetworkListManager` (NLA API)
  - Pre-logon CP: blocks Connect with status message when corporate network detected
  - Post-logon tray: auto-disconnect + balloon notification

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
  - Compact traffic format (`79.6M` instead of `79.6 MB`) to fit 128-char limit

### Changed

- `HandshakeTimeoutSec` registry key written by installer (not `configure.reg`)
- All registry keys written directly by NSI installer (`configure.reg` removed)
- New keys checked individually on update, added if missing
- `WGGetLastHandshakeSec`: uses stdout pipe instead of `cmd.exe` redirect
- Traffic format: `79.6M` / `43.2M` (compact, no space before unit)
- Tray manifest: `asInvoker` -> `requireAdministrator`
- Log encoding: unified UTF-8, no separate `_cp.log` suffix

### Fixed

- Yubico Authenticator path corrected to
  `C:\Program Files\Yubico\Yubico Authenticator\authenticator.exe`
- Yubico Authenticator launched via `CreateProcess` (inherits admin token, works
  without UAC)
- Double space in tooltip `Handshake  pending` -> `Handshake pending`
- Tray tooltip truncated at 128 chars (traffic line was cut off)
- Tooltip icon `\u1F512` corrected to `\U0001F511`

---

## [2026.7.31] - 2026-07-31

### Added

- **Pre-logon YubiKey PIV authentication** fully functional
  - PIN entry field directly in the credential tile (`CPFT_PASSWORD_TEXT`, masked)
  - PIN extracted via `CredUnPackAuthenticationBufferW` in `GetSerialization`
  - Certificate read from YubiKey via PIV GET DATA APDU with GET RESPONSE chaining
  - RSA2048 certificates (>256 bytes) now fully retrieved via `SW=61xx` chaining
  - SELECT PIV AID sent before GET DATA (required for UICC/NFC context)
  - Separate CP log file: `C:\Windows\Temp\wgcp_ddMMyyyy_cp.log`
  - Log directory fallback to `C:\Windows\Temp` when SYSTEM cannot write to INSTDIR

- **`installer\Setup-YubiKey.ps1`** — interactive YubiKey PIV setup tool
  - Menu: Initialize / Register existing YubiKey / Export report
  - Full PIV reset, random 8-digit PIN/PUK, RSA2048 key + self-signed cert (10 y)
  - AD Distinguished Name lookup (`Get-ADUser`) with manual fallback
  - Writes `SmartcardEnabled=1` and `SmartcardCertThumbprint` to registry
  - Setup report (PIN/PUK/thumbprint) saved to user-defined path

### Changed

- All user-facing "Smartcard" labels replaced with "YubiKey" (pre-logon tile and tray)
- PIN field in pre-logon tile: shown when disconnected, hidden when connected
- `GetSerialization` triggers `CommandLinkClicked` when user presses Enter
- SC-Watch thread: detects card present at startup, updates status on every tick
- Log encoding changed from UTF-16LE to UTF-8 (no BOM, readable with standard tools)
- Default log level changed back to CRIT (1) — DEBUG only when explicitly configured
- Version bumped to 2026.7.31 in all RC files

### Fixed

- Pre-logon PIN was never received (`CPFT_PASSWORD_TEXT` does not call `SetStringValue`)
- YubiKey RSA2048 certificate truncated at 258 bytes — fixed via GET RESPONSE chaining
- `SHCreateDirectoryExW` failure silently prevented all CP logging as SYSTEM
- CP and tray wrote to same log file in `C:\Windows\Temp`
- `CERT_STORE_PROV_SMARTCARD` not available in modern SDK — replaced with direct PIV
  GET DATA APDU
- `SCardFreeMemory(nullptr)` compilation error — removed unnecessary reader status call
- Leftover `CredLog` call causing compilation error after debug cleanup

---

## [2026.7.30] - 2026-07-30

### Added

- **Post-logon tray application** (`WireGuardCPTray.exe`)
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

- Various tray, installer, and resource fixes (see full entry above)

---

## [2026.7.6] - 2026-07-29

### Changed

- Registry key moved to
  `HKEY_LOCAL_MACHINE\SOFTWARE\Jens Kaesler\WireGuard Credential Provider`
- All source code comments, log messages, and UI strings fully translated to English

### Fixed

- `min()` call in smartcard PIN copy replaced with explicit ternary
- Duplicate `SetStringValue` stub removed

---

## [2026.7.14] - 2026-07-25

### Added

- Comprehensive debug logging across all smartcard functions

---

## [2026.7.13] - 2026-07-25

### Added

- **Smartcard / YubiKey PIV authentication** — optional second factor before connecting

---

## [2026.7.12] - 2026-07-24

### Changed

- General code cleanup and refactoring ahead of smartcard feature

---

## [2026.7.11] - 2026-07-24

### Fixed

- Installer: robocopy, WOW64 redirection, LogonUI kill, System32 path fixes

---

## [2026.7.5] - 2026-07-24

### Added

- Log rotation, daily log files, `InstallDir` registry base path

---

## [2026.7.4] - 2026-07-23

### Changed

- `FieldDescriptors.h` extracted; `volatile` fields; shutdown service fixes

---

## [2026.7.3] - 2026-07-23

### Added

- **WireGuardShutdownService**: `SERVICE_CONTROL_PRESHUTDOWN` handler

---

## [2026.7.2] - 2026-07-23

### Added

- Auto-refresh, connection timer, traffic statistics, color-coded icons

---

## [2026.7.1] - 2026-07-23

### Added

- Profile dropdown, connect/disconnect button, status display, service detection

---

## [2026.7.0] - 2026-07-23

### Added

- Initial release: WireGuard Credential Provider DLL (`ICredentialProvider`)
