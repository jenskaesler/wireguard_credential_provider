# 🔐 WireGuard Credential Provider

> Connect your WireGuard VPN directly from the Windows login screen – secured by YubiKey PIV.

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)
[![Version](https://img.shields.io/badge/Version-2026.8.1-blue)](CHANGELOG.md)
[![WireGuard](https://img.shields.io/badge/WireGuard-Windows-88171A?logo=wireguard&logoColor=white)](https://www.wireguard.com/install/)

---

## 🤔 Why does this exist?

Domain-joined Windows clients face a classic chicken-and-egg problem: the user needs to log in to establish a VPN connection – but the VPN connection is required to reach the domain controller for authentication.

Many commercial VPN clients solve this with a "pre-logon" feature. **WireGuard for Windows does not offer this functionality** – until now.

The **WireGuard Credential Provider** adds a dedicated tile to the Windows login screen, allowing tunnels to be started and stopped directly before signing in. It also ships a **Post-Logon Tray Application** that replaces the WireGuard UI entirely for domain-managed machines.

> **Based on:** [WireGuard Credential Provider UI](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/CredentialProvider) – the credential provider shell architecture follows the Microsoft Windows Classic Samples reference implementation.

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
- 📋 **Profile management** – multi-profile submenu, active profile shown in menu header
- 🔒 **WireGuard UI blocker** – detects and silently terminates the WireGuard UI
- 🔄 **Shortcut watchdog** – automatically removes the WireGuard Start Menu shortcut after updates
- 📥 **Profile import** – imports `.conf` files into the WireGuard configuration directory
- 🌍 **Bilingual** – German and English UI based on the Windows system locale
- 🌙 **Dark Mode aware** – reads Windows theme preference and applies it to menus
- 🪪 **YubiKey PIV** – same authentication gate as the pre-logon tile
- 🔌 **Auto-disconnect** – disconnects tunnel when YubiKey is removed
- 🤝 **Handshake watchdog** – disconnects if WireGuard handshake exceeds configurable timeout
- 🏢 **Corporate network detection** – auto-disconnects when domain network is detected (NLA)
- 🖱️ **Left-click toggle** – click tray icon to connect/disconnect
- 📊 **Rich tooltip** – status, profile, uptime, handshake age, traffic stats on hover

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
- Resets the PIV application
- Generates a random 8-digit PIN and PUK
- Creates an RSA2048 key pair in slot 9a
- Issues a self-signed certificate (10 year validity)
- Writes `SmartcardCertThumbprint` and `SmartcardEnabled=1` to the registry
- Saves a setup report (including PIN/PUK) to a user-defined location

> **Compatible YubiKey models:** 5 NFC, 5C, 5Ci, 5 Nano, 5C NFC, 5C Nano  
> **Not compatible:** YubiKey Bio, Security Key series, YubiKey 4 series

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
| `HandshakeTimeoutSec` | REG_DWORD | Disconnect if handshake older than N seconds (0=off) | `0` |
| `LogLevel` | REG_DWORD | `0`=off `1`=CRIT `2`=WARN `3`=DEBUG | `1` |
| `LogRetentionDays` | REG_DWORD | Auto-delete logs older than N days | `7` |
| `ExePath` | REG_SZ | Path to `wireguard.exe` | `C:\Program Files\WireGuard\wireguard.exe` |
| `ConfigDir` | REG_SZ | WireGuard configuration directory | `C:\Program Files\WireGuard\Data\Configurations\` |

---

## 🐛 Troubleshooting

Enable verbose logging:
```reg
[HKEY_LOCAL_MACHINE\SOFTWARE\Jens Kaesler\WireGuard Credential Provider]
"LogLevel"=dword:00000003
```

Log files:
- **Tray App:** `INSTDIR\logs\wgcp_ddMMyyyy.log`
- **CP DLL (pre-logon):** `C:\Windows\Temp\wgcp_ddMMyyyy_cp.log`

---

## 📜 License

[MIT](LICENSE)
