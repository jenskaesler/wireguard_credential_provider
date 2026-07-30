# 🔐 WireGuard Credential Provider

> Connect your WireGuard VPN directly from the Windows login screen – before you log in.

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)
[![Version](https://img.shields.io/badge/Version-2026.7.30-blue)](CHANGELOG.md)
[![WireGuard](https://img.shields.io/badge/WireGuard-Windows-88171A?logo=wireguard&logoColor=white)](https://www.wireguard.com/install/)

---

## 🤔 Why does this exist?

Domain-joined Windows clients face a classic chicken-and-egg problem: the user needs to log in to establish a VPN connection – but the VPN connection is required to reach the domain controller for authentication.

Many commercial VPN clients solve this with a "pre-logon" feature. **WireGuard for Windows does not offer this functionality** – until now.

The **WireGuard Credential Provider** adds a dedicated tile to the Windows login screen, allowing tunnels to be started and stopped directly before signing in. It also ships a **Post-Logon Tray Application** that replaces the WireGuard UI entirely for domain-managed machines.

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
- 🪪 **Smartcard / YubiKey PIV** – optional PIN + certificate thumbprint authentication

### Post-Logon (Tray Application)
- 🖥️ **System tray icon** – color-coded lock icon (green/red) reflecting tunnel state
- 📋 **Profile management** – multi-profile submenu, active profile shown in menu header
- 🔒 **WireGuard UI blocker** – detects and silently terminates the WireGuard UI (no flash)
- 🔄 **Shortcut watchdog** – automatically removes the WireGuard Start Menu shortcut after updates
- 📥 **Profile import** – imports `.conf` files into the WireGuard configuration directory
- 🌍 **Bilingual** – German and English UI based on the Windows system locale
- 🌙 **Dark Mode aware** – reads Windows theme preference and applies it to menus
- 🪪 **Smartcard / YubiKey PIV** – same authentication gate as the pre-logon tile

### Shared
- 🛑 **Clean shutdown** – `WireGuardShutdownHelper` service disconnects all tunnels on shutdown
- ⚙️ **Single Registry key** – one key controls both pre-logon and post-logon behavior
- 📝 **Structured logging** – configurable log levels (CRIT / WARN / DEBUG), daily log files
- 🏢 **GPO-ready** – all settings distributable via Group Policy Preferences

---

## 📋 Prerequisites

| Component | Version |
|---|---|
| Windows | 10 or 11 (x64) |
| WireGuard for Windows | [latest](https://www.wireguard.com/install/) – installed automatically by the installer |
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
│   ├── helpers.h                       – Shared: Registry, logging, WireGuard, smartcard
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
│   │   ├── WireGuardTray.rc            – Icon resources (app icon + tray icons)
│   │   ├── WireGuardTray.manifest      – DPI awareness, visual styles
│   │   ├── icon.ico                    – Application icon (shown in taskbar/autostart)
│   │   ├── wireguard_tray_connected.ico    – Green lock (tunnel up)
│   │   └── wireguard_tray_disconnected.ico – Red lock (tunnel down)
│   └── WireGuardTray.vcxproj
│
├── resources/
│   ├── resource.h                      – Resource IDs (shared by CP DLL and Tray)
│   ├── WireGuardCredentialProvider.rc  – CP DLL resource script
│   ├── wireguard_connected.bmp         – CP tile icon connected (128×128)
│   └── wireguard_disconnected.bmp      – CP tile icon disconnected (128×128)
│
├── shutdown-service/
│   └── src/WireGuardShutdownService.cpp – Preshutdown service
│
├── installer/
│   ├── WireGuardCredentialProvider.nsi – NSIS installer (WireGuard auto-install, YubiKey tools)
│   ├── build.bat                       – Copies binaries, runs NSIS
│   └── content/                        – Installer assets (icons, docs, configure.reg)
│
├── WireGuardCredentialProvider.sln
├── WireGuardCredentialProvider.vcxproj
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

Open `WireGuardCredentialProvider.sln` in Visual Studio → **Release | x64** → **Ctrl+Shift+B**

Three projects are built:
- `x64\Release\WireGuardCredentialProvider.dll`
- `x64\Release\WireGuardShutdownService.exe`
- `tray-app\x64\Release\WireGuardCPTray.exe`

### 3. Create installer

```cmd
installer\build.bat
```

Copies binaries to `installer\content\` and compiles the NSIS setup.

### 4. Install

Run `Setup_WireGuardCredentialProvider_x64.exe` as Administrator.

The installer:
- Downloads and silently installs WireGuard if not already present
- Copies DLL and services to `System32`
- Registers the Credential Provider via `regsvr32`
- Writes default Registry configuration
- Installs `WireGuardShutdownHelper` service
- Sets autostart for `WireGuardCPTray.exe`
- Removes the WireGuard Start Menu shortcut (backed up in `INSTDIR\backup\`)
- Optionally installs YubiKey tools (Authenticator, Minidriver, Manager CLI)

### 5. Silent deployment (Baramundi / MDM)

```cmd
:: Minimal install (CP + Tray only)
Setup_WireGuardCredentialProvider_x64.exe /S

:: Full install including all YubiKey tools
Setup_WireGuardCredentialProvider_x64.exe /S /FULL

:: Custom – select individual YubiKey components
Setup_WireGuardCredentialProvider_x64.exe /S /YKAUTH /YKMINI /YKMGRCLI

:: Silent uninstall
Uninstall.exe /S
```

---

## ⚙️ Configuration

All settings: `HKEY_LOCAL_MACHINE\SOFTWARE\Jens Kaesler\WireGuard Credential Provider`

| Value | Type | Description | Default |
|---|---|---|---|
| `ExePath` | REG_SZ | Path to `wireguard.exe` | `C:\Program Files\WireGuard\wireguard.exe` |
| `WgExePath` | REG_SZ | Path to `wg.exe` (traffic stats) | `C:\Program Files\WireGuard\wg.exe` |
| `ConfigDir` | REG_SZ | WireGuard configuration directory | `C:\Program Files\WireGuard\Data\Configurations\` |
| `TileLabel` | REG_SZ | Heading on the login tile | `WireGuard VPN` |
| `LogPath` | REG_SZ | Log file path | `INSTDIR\logs\wgcp_ddMMyyyy.log` |
| `LogLevel` | REG_DWORD | `0`=off `1`=CRIT `2`=WARN `3`=DEBUG | `1` |
| `LogRetentionDays` | REG_DWORD | Auto-delete logs older than N days | `7` |
| `InstallDir` | REG_SZ | Set by installer | *(automatic)* |
| `SmartcardEnabled` | REG_DWORD | Enable smartcard / YubiKey PIV | `0` |
| `SmartcardPinRequired` | REG_DWORD | Require PIN | `1` |
| `SmartcardPinMinLength` | REG_DWORD | Minimum PIN length | `4` |
| `SmartcardPinMaxAttempts` | REG_DWORD | Max failed attempts | `3` |
| `SmartcardTimeout` | REG_DWORD | Seconds to wait for card | `10` |
| `SmartcardConnectOnInsert` | REG_DWORD | Auto-connect on card insert | `0` |
| `SmartcardDisconnectOnRemove` | REG_DWORD | Auto-disconnect on card removal | `0` |
| `SmartcardReaderName` | REG_SZ | Restrict to reader name (empty = any) | *(empty)* |
| `SmartcardCertThumbprint` | REG_SZ | Expected SHA-1 thumbprint | *(empty)* |

> **`SmartcardEnabled`** controls **both** the pre-logon tile and the post-logon tray – one switch for everything.

---

## 🪪 Smartcard / YubiKey PIV

When enabled, the credential provider and tray application both require smartcard authentication before connecting a tunnel:

1. Wait for a PIV card/YubiKey to be present (configurable timeout)
2. Optionally verify the certificate thumbprint (SHA-1)
3. Optionally verify the PIV PIN via VERIFY APDU (ISO 7816-4, slot 80h)

Compatible with YubiKey 5 series and any CCID/PIV device. Not compatible with YubiKey Bio or Security Key series.

---

## 🛑 Shutdown Service

`WireGuardShutdownHelper` registers for `SERVICE_CONTROL_PRESHUTDOWN` and disconnects all active tunnels before Windows shuts down (30-second timeout).

---

## 🐛 Troubleshooting

Enable verbose logging:
```reg
[HKEY_LOCAL_MACHINE\SOFTWARE\Jens Kaesler\WireGuard Credential Provider]
"LogLevel"=dword:00000003
```
Logs are written to `INSTDIR\logs\wgcp_ddMMyyyy.log`. Reset to `1` after diagnostics.

---

## 📜 License

[MIT](LICENSE)
