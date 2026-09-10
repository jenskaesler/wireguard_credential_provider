# 🔐 WireGuard Credential Provider

> Connect your WireGuard VPN directly from the Windows login screen – secured by YubiKey PIV.

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)
[![Version](https://img.shields.io/badge/Version-2026.9.9-blue)](CHANGELOG.md)
[![WireGuard](https://img.shields.io/badge/WireGuard-Windows-88171A?logo=wireguard&logoColor=white)](https://www.wireguard.com/install/)

---

## 🤔 Why does this exist?

Domain-joined Windows clients face a classic chicken-and-egg problem: the user needs to log in to establish a VPN connection – but the VPN connection is required to reach the domain controller for authentication.

Many commercial VPN clients solve this with a "pre-logon" feature. **WireGuard for Windows does not offer this functionality** – until now.

The **WireGuard Credential Provider** adds a dedicated tile to the Windows login screen, allowing tunnels to be started and stopped directly before signing in. It also ships a **Post-Logon Tray Application** that replaces the WireGuard UI entirely for domain-managed machines.

> **Based on:** [WireGuard Credential Provider UI](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/CredentialProvider) – the credential provider shell architecture follows the Microsoft Windows Classic Samples reference implementation.
>
> **WireGuard for Windows:** This project integrates with [WireGuard for Windows](https://github.com/WireGuard/wireguard-windows) – the official WireGuard client by Jason A. Donenfeld. The tunnel management (`wireguard.exe /installtunnelservice`, `wg.exe show`) and the `.conf.dpapi` configuration format are part of that project.

---

## 🔑 YubiKey PIV Support

This version supports **YubiKey 5 Series** (YubiKey 5 NFC, 5C, 5Ci, 5 Nano, etc.) with PIV (Personal Identity Verification) authentication.

> **Note:** Only YubiKey PIV is supported in this version. Other smartcard readers, PIV cards from other vendors, or YubiKey Bio / Security Key series are **not supported**.

Authentication flow:
1. YubiKey detected in reader
2. Certificate thumbprint verified against registry configuration
3. PIN verified via PIV VERIFY APDU (ISO 7816-4, slot 80h)
4. WireGuard tunnel connects

---

## ✨ Features

### Pre-Logon (Credential Provider DLL)
- 🔌 **Pre-Logon VPN** – connect and disconnect tunnels before Windows login
- 📋 **Profile selection** – dropdown listing all available `.conf.dpapi` configurations
- 🎯 **Automatic default profile** – detects a configuration matching the computer name
- 🟢🔴 **Color-coded status icons** – green when connected, red when disconnected
- ⏱️ **Live connection timer** – shows how long the tunnel has been active
- 📊 **Traffic statistics** – real-time upload/download throughput via `wg.exe`
- 🔄 **Auto-refresh** – status and traffic update every 5 seconds
- 🪪 **YubiKey PIV** – PIN + certificate thumbprint authentication

### Post-Logon (Tray Application)
- 🖥️ **System tray icon** – color-coded lock icon (green/red) reflecting tunnel state
- 📋 **Context menu** – Connect/Disconnect directly in main menu; per-profile submenus with Connect / Disconnect / Delete; active profile shown with ✓ checkmark and 🟢 dot
- 🖱️ **Click behavior** – single click and double click both toggle connect/disconnect (one action, no duplicate triggers); left-click on profile entry selects without connecting
- 🔒 **WireGuard UI blocker** – detects and silently terminates the WireGuard UI
- 🔄 **Shortcut watchdog** – automatically removes the WireGuard Start Menu shortcut after updates
- 📥 **Profile import** – imports `.conf` files, triggers WireGuardManager briefly to encrypt to `.conf.dpapi`, then removes the plain-text file; tray menu refreshes immediately after import
- ✏️ **Profile editor** – edit any profile directly from the tray submenu; decrypts `.conf.dpapi` via the SYSTEM helper service, opens an embedded text editor, re-encrypts via WireGuardManager on save (same flow as import, guaranteed compatibility)
- 💾 **Profile export** – export any profile to a plain-text `.conf` file via Save dialog; decryption handled by the SYSTEM helper service
- 🌍 **Bilingual** – German and English UI based on the Windows system locale
- 🌙 **Dark Mode** – full dark mode support for context menus and submenus (comctl32 v6, `AllowDarkModeForWindow`, `WH_CALLWNDPROC` hook)
- 🪪 **YubiKey PIV** – same authentication gate as the pre-logon tile
- 🔌 **Auto-disconnect** – disconnects tunnel when YubiKey is removed
- 🤝 **Handshake watchdog** – disconnects if WireGuard handshake exceeds configurable timeout (default: 180 s); suppressed on corporate network where no handshake is expected
- 🏢 **Corporate network detection** – auto-disconnects when the machine is physically on the corporate network
  - **Stage 1:** Windows NLA (`INetworkListManager`) – detects `DOMAIN_AUTHENTICATED` networks
  - **Stage 2 fallback:** domain-join registry check + `DsGetDcName` + **route verification via `GetBestRoute`** – correctly handles VMs (Red Hat VirtIO, Hyper-V) where NLA reports `PRIVATE` instead of `DOMAIN_AUTHENTICATED`, and rejects the case where the DC is only reachable via the WireGuard tunnel (Homeoffice VPN)
  - WireGuard virtual adapters excluded from both stages to prevent false positives
  - Disconnect on every NetWatch tick when corporate + connected (not only on state transition)
- 📊 **Rich tooltip** – status, profile, uptime, handshake age, traffic stats on hover

### Shared
- 🛑 **Clean shutdown** – `WireGuardShutdownHelper` service disconnects all tunnels on shutdown
- ⚙️ **Single Registry key** – one key controls both pre-logon and post-logon behavior
- 📝 **Structured logging** – configurable log levels (CRIT / WARN / DEBUG), daily log files
- 🏢 **GPO-ready** – all settings distributable via Group Policy Preferences
- 🔑 **Admin autostart** – tray starts as Administrator via CommonStartup shortcut (no UAC prompt)

---

## 📋 Prerequisites

| Component | Version |
|---|---|
| Windows | 10 or 11 (x64) |
| WireGuard for Windows | [latest](https://www.wireguard.com/install/) – installed automatically by the installer |
| YubiKey | 5 Series (5 NFC, 5C, 5Ci, 5 Nano, 5C NFC) |
| YubiKey Manager CLI | v5.9.2+ (`ykman`) – for `Setup-YubiKey.ps1` |
| Visual Studio | 2022 or newer (toolset v145) |
| Windows SDK | 10.0 |
| C++ Workload | Desktop development with C++ |
| NSIS | 3.x with **inetc**, **nsProcess**, **SimpleSC** plugins |

---

## 🏗️ Project Structure

```
WireGuardCredentialProvider/
│
├── src/
│   ├── helpers.h                       – Shared: Registry, logging, WireGuard, YubiKey PIV
│   ├── FieldDescriptors.h              – Field definitions (CP DLL only)
│   ├── WireGuardProvider.h/.cpp        – ICredentialProvider implementation
│   ├── WireGuardCredential.h/.cpp      – ICredentialProviderCredential implementation
│   ├── dll.cpp                         – DllMain, DllGetClassObject, regsvr32 exports
│   └── WireGuardCredentialProvider.def – DLL export definitions
│
├── tray-app/
│   ├── src/
│   │   ├── WireGuardTray.h/.cpp        – Post-logon tray application
│   │   └── main.cpp                    – Entry point, dark mode, single-instance guard
│   ├── resources/
│   │   ├── WireGuardTray.rc
│   │   ├── icon.ico
│   │   ├── wireguard_tray_connected.ico
│   │   └── wireguard_tray_disconnected.ico
│   └── WireGuardTray.vcxproj
│
├── resources/
│   ├── resource.h
│   ├── WireGuardCredentialProvider.rc
│   ├── wireguard_connected.bmp
│   └── wireguard_disconnected.bmp
│
├── shutdown-service/
│   └── src/WireGuardShutdownService.cpp
│
├── installer/
│   ├── WireGuardCredentialProvider.nsi – NSIS installer
│   ├── Setup-YubiKey.ps1               – YubiKey PIV setup and registration tool
│   ├── build.bat
│   └── content/
│
├── WireGuardCredentialProvider.sln
├── CHANGELOG.md
├── LICENSE
└── README.md
```

---

## ⚡ Quick Start

### 1. Clone

```cmd
git clone https://github.com/jenskaesler/wireguard_credential_provider.git
cd wireguard_credential_provider
```

### 2. Build

Open `WireGuardCredentialProvider.sln` → **Release | x64** → **Ctrl+Shift+B**

### 3. Create installer

```cmd
installer\build.bat
```

### 4. Install

Run `Setup_WireGuardCredentialProvider_x64.exe` as Administrator.

### 5. Silent deployment (Baramundi / MDM)

```cmd
Setup_WireGuardCredentialProvider_x64.exe /S
Setup_WireGuardCredentialProvider_x64.exe /S /FULL
Setup_WireGuardCredentialProvider_x64.exe /S /YKAUTH /YKMINI /YKMGRCLI
Uninstall.exe /S
```

---

## 🪪 YubiKey PIV Setup

Use the included `installer\Setup-YubiKey.ps1` (requires `ykman` and Administrator rights):

```powershell
# Run as Administrator
Set-ExecutionPolicy Bypass -Scope Process
.\installer\Setup-YubiKey.ps1
```

**Menu options:**

| Option | Description |
|---|---|
| **1 – Initialize YubiKey** | Full PIV reset, generate key + certificate, write thumbprint to registry |
| **2 – Register existing YubiKey** | Read thumbprint from inserted YubiKey, write to registry |
| **3 – Export setup report** | Re-create setup report from inserted YubiKey |

The script:
- Validates prerequisites (`ykman` present, Administrator rights, YubiKey connected, PIV enabled)
- Resets the PIV application
- Generates a random 8-digit PIN and PUK
- Creates an RSA2048 key pair in slot 9a
- Issues a self-signed certificate (10 year validity)
- Writes `SmartcardCertThumbprint` and `SmartcardEnabled=1` to the registry
- Saves a setup report (including PIN/PUK) to a user-defined location
- Returns to the menu on error (no `exit 1`) – retry without restarting the script

> **Compatible YubiKey models:** 5 NFC, 5C, 5Ci, 5 Nano, 5C NFC, 5C Nano
> **Not compatible:** YubiKey Bio, Security Key series, YubiKey 4 series

---

## 📦 Installer Documentation

For silent deployment parameters, MDM integration and YubiKey setup see [`installer/README.md`](installer/README.md).

---

## ⚙️ Configuration

All settings: `HKEY_LOCAL_MACHINE\SOFTWARE\Jens Kaesler\WireGuard Credential Provider`

| Value | Type | Description | Default |
|---|---|---|---|
| `SmartcardEnabled` | REG_DWORD | Enable YubiKey PIV authentication | `0` |
| `SmartcardPinRequired` | REG_DWORD | Require PIN entry | `1` |
| `SmartcardPinMinLength` | REG_DWORD | Minimum PIN length | `4` |
| `SmartcardCertThumbprint` | REG_SZ | Expected certificate SHA-1 thumbprint | *(empty)* |
| `SmartcardDisconnectOnRemove` | REG_DWORD | Auto-disconnect when YubiKey removed | `0` |
| `SmartcardConnectOnInsert` | REG_DWORD | Auto-connect when YubiKey inserted | `0` |
| `SmartcardReaderName` | REG_SZ | Restrict to specific reader name | *(empty)* |
| `HandshakeTimeoutSec` | REG_SZ | Disconnect if no handshake for N seconds (`"0"` = off) | `"180"` |
| `AutoUpdateCheck` | REG_DWORD | Check GitHub for new releases on startup (`1`=on, `0`=off) | `1` |
| `LogLevel` | REG_SZ | `0`=off `1`=CRIT `2`=WARN `3`=DEBUG | `"1"` |
| `LogRetentionDays` | REG_SZ | Auto-delete logs older than N days | `"7"` |
| `ExePath` | REG_SZ | Path to `wireguard.exe` | `C:\Program Files\WireGuard\wireguard.exe` |
| `ConfigDir` | REG_SZ | WireGuard configuration directory | `C:\Program Files\WireGuard\Data\Configurations\` |

> **Note:** `LogLevel`, `LogRetentionDays` and `HandshakeTimeoutSec` are stored as `REG_SZ`.
> Enter the decimal number as text in Registry Editor – no hex conversion needed.

---

## 🐛 Troubleshooting

Enable verbose logging:
```reg
[HKEY_LOCAL_MACHINE\SOFTWARE\Jens Kaesler\WireGuard Credential Provider]
"LogLevel"=dword:00000003
```

Log files:
- **Tray App:** `INSTDIR\logs\wgcp_ddMMyyyy.log`
- **CP DLL (pre-logon):** `INSTDIR\logs\wgcp_ddMMyyyy.log` (same file)

**Corporate network detection not working on VM?**

On VMs with Red Hat VirtIO or Hyper-V adapters, Windows NLA may classify the network as
`PRIVATE` instead of `DOMAIN_AUTHENTICATED`. The Stage 2 fallback handles this automatically:
it checks domain-join status, adapter connectivity, and calls `DsGetDcName` to verify DC
reachability on the LAN. Enable `LogLevel=3` and look for `CorpNet: Stage2` log entries.

**VPN disconnects immediately after connecting from Homeoffice?**

This was a known bug (fixed in 2026.8.4). The route to the DC now goes through a route
check (`GetBestRoute`): if the DC is only reachable via the WireGuard adapter, Stage 2
returns `false` (not corporate) and the tunnel stays up.

**Installer shows "file in use" dialog for WireGuardCPTray.exe?**

This was fixed in 2026.8.4. The installer now uses `nsProcess::KillProcess` and waits
with an active poll loop until the process is gone before copying new binaries.

---

## 📜 License

[MIT](LICENSE)
