# 🔐 WireGuard Credential Provider

> Connect your WireGuard VPN directly from the Windows login screen – before you log in.

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)
[![Version](https://img.shields.io/badge/Version-2026.7.6-blue)](CHANGELOG.md)
[![WireGuard](https://img.shields.io/badge/WireGuard-Windows-88171A?logo=wireguard&logoColor=white)](https://www.wireguard.com/install/)

---

## 🤔 Why does this exist?

Domain-joined Windows clients face a classic chicken-and-egg problem: the user needs to log in to establish a VPN connection – but the VPN connection is required to reach the domain controller for authentication.

Many commercial VPN clients solve this with a "pre-logon" feature. **WireGuard for Windows does not offer this functionality** – until now.

The **WireGuard Credential Provider** adds a dedicated tile to the Windows login screen, allowing tunnels to be started and stopped directly before signing in.

---

## ✨ Features

- 🔌 **Pre-Logon VPN** – connect and disconnect tunnels before Windows login
- 📋 **Profile selection** – dropdown listing all available `.conf.dpapi` configurations
- 🎯 **Automatic default profile** – detects a configuration matching the computer name (e.g. `LT260430.conf.dpapi`)
- 🟢🔴 **Color-coded status icons** – green icon when connected, red when disconnected
- ⏱️ **Live connection timer** – shows how long the tunnel has been active
- 📊 **Traffic statistics** – real-time upload/download throughput (via `wg.exe`)
- 🔄 **Auto-refresh** – status and traffic update automatically every 5 seconds
- 🛑 **Clean shutdown** – a separate Windows service disconnects all tunnels on shutdown
- ⚙️ **Fully configurable** – all paths and labels via the Windows Registry
- 📝 **Structured logging** – configurable log levels (CRIT / WARN / DEBUG)
- 🏢 **GPO-ready** – Registry configuration can be distributed via Group Policy

---

## 📋 Prerequisites

| Component | Version |
|---|---|
| Windows | 10 or 11 (x64) |
| WireGuard for Windows | [latest version](https://www.wireguard.com/install/) |
| Visual Studio | 2022 or newer |
| Windows SDK | 10.0 |
| C++ Workload | Desktop development with C++ |

---

## 🏗️ Project Structure

```
WireGuardCredentialProvider/
│
├── src/
│   ├── helpers.h                       – Registry helpers, logging, WireGuard functions
│   ├── FieldDescriptors.h              – Central field definitions (no duplication)
│   ├── WireGuardProvider.h/.cpp        – ICredentialProvider (frame, enumeration)
│   ├── WireGuardCredential.h/.cpp      – ICredentialProviderCredential (tile, logic)
│   ├── dll.cpp                         – DllMain, DllGetClassObject, regsvr32 exports
│   └── WireGuardCredentialProvider.def – DLL export definitions
│
├── resources/
│   ├── resource.h                      – Resource IDs
│   ├── WireGuardCredentialProvider.rc  – Resource script (embedded icons)
│   ├── wireguard_connected.bmp         – Icon when connected (128×128, green)
│   └── wireguard_disconnected.bmp      – Icon when disconnected (128×128, red)
│
├── shutdown-service/
│   ├── WireGuardShutdownService.vcxproj
│   └── src/
│       └── WireGuardShutdownService.cpp – Windows service for clean shutdown
│
├── installer/
│   └── content/
│       └── configure.reg               – Registry configuration (imported on install)
│
├── WireGuardCredentialProvider.sln
├── WireGuardCredentialProvider.vcxproj
├── CHANGELOG.md
├── LICENSE
└── README.md
```

---

## ⚡ Quick Start

### 1. Clone the repository

```cmd
git clone https://github.com/jenskaesler/wireguard-credential-provider.git
cd wireguard-credential-provider
```

### 2. Build

Open Visual Studio → `WireGuardCredentialProvider.sln` → **Release | x64** → **Ctrl+Shift+B**

Both projects are built:
- `x64\Release\WireGuardCredentialProvider.dll`
- `shutdown-service\x64\Release\WireGuardShutdownService.exe`

### 3. Prepare outputs

```cmd
copy x64\Release\WireGuardCredentialProvider.dll installer\content\
copy shutdown-service\x64\Release\WireGuardShutdownService.exe installer\content\
```

### 4. Install

```cmd
:: Run as Administrator:
installer\install.bat
```

The script will:
- Copy the DLL to `%SystemRoot%\System32\`
- Register the Credential Provider via `regsvr32`
- Import the default configuration from `configure.reg`
- Install and start the `WireGuardShutdownHelper` service

### 5. Test

Lock the screen (`Win+L`) → the WireGuard tile appears alongside the user tiles.

---

## ⚙️ Configuration

All settings are stored under `HKEY_LOCAL_MACHINE\SOFTWARE\Jens Kaesler\WireGuard Credential Provider`:

| Value | Type | Description | Default |
|---|---|---|---|
| `ExePath` | REG_SZ | Path to `wireguard.exe` | `C:\Program Files\WireGuard\wireguard.exe` |
| `WgExePath` | REG_SZ | Path to `wg.exe` (traffic stats) | `C:\Program Files\WireGuard\wg.exe` |
| `TileLabel` | REG_SZ | Heading on the tile | `WireGuard VPN` |
| `IconConnected` | REG_SZ | Custom icon (connected, 128×128 BMP) | *(embedded resource)* |
| `IconDisconnected` | REG_SZ | Custom icon (disconnected, 128×128 BMP) | *(embedded resource)* |
| `LogPath` | REG_SZ | Path to the log file | `C:\wgcp_debug.log` |
| `LogLevel` | REG_DWORD | Log level | `1` (CRIT) |
| `LogRetentionDays` | REG_DWORD | Log retention in days | `7` |
| `InstallDir` | REG_SZ | Installation directory (set by installer) | *(set automatically)* |

**Log levels:**
- `0` – Logging disabled (production)
- `1` – Critical errors only
- `2` – Warnings and errors
- `3` – Everything (diagnostic)

### Distribute configuration via GPO

The Registry values can be pushed to all domain clients using **Group Policy Preferences (GPP)**. The DLL and the shutdown service are distributed via a logon script or software deployment tool.

### Custom icons

Icons must be **128×128 pixels, 24bpp, uncompressed BMP**. If `IconConnected` and `IconDisconnected` are set in the Registry, these will be used instead of the embedded resources.

---

## 🛑 Shutdown Behavior

The **WireGuard Shutdown Helper** (`WireGuardShutdownService.exe`) runs as a Windows service (startup type: Automatic). It registers for `SERVICE_CONTROL_PRESHUTDOWN` – Windows notifies it before the actual shutdown so all active tunnels can be cleanly disconnected (timeout: 30 seconds).

```cmd
:: Check status
sc query WireGuardShutdownHelper

:: Manual test – disconnects all active tunnels immediately
WireGuardShutdownService.exe /run

:: Uninstall
WireGuardShutdownService.exe /uninstall
```

---

## 🔧 Technical Details

### Architecture

The Credential Provider is a **COM in-process DLL** loaded by `LogonUI.exe` on the `Winsta0\Winlogon` desktop. It implements:

- `ICredentialProvider` – Registration and enumeration of tiles
- `ICredentialProviderCredential` – Tile rendering and interaction

The tile does not return any credential serialization (`CPGSR_NO_CREDENTIAL_NOT_FINISHED`) – it is solely used for VPN control and leaves the normal Windows login flow untouched.

### WireGuard Integration

| Action | Command |
|---|---|
| Connect tunnel | `wireguard.exe /installtunnelservice "C:\...\Profile.conf.dpapi"` |
| Disconnect tunnel | `wireguard.exe /uninstalltunnelservice ProfileName` |
| Connection status | Query service `WireGuardTunnel$ProfileName` |
| Traffic stats | `wg.exe show ProfileName transfer` |

### Configuration Directory

WireGuard stores encrypted configurations under:
```
C:\Program Files\WireGuard\Data\Configurations\*.conf.dpapi
```
The Credential Provider reads the file names (without `.conf.dpapi`) to populate the profile dropdown. The content of the encrypted files is never read – `wireguard.exe` handles decryption internally.

### Security Note

Both the Credential Provider and the Shutdown Service run as **SYSTEM**. The Registry key `HKLM\SOFTWARE\Jens Kaesler\WireGuard Credential Provider` is by default writable only by administrators – make sure this remains the case in your environment.

---

## 🐛 Troubleshooting

Set `LogLevel` to `3`:

```reg
[HKEY_LOCAL_MACHINE\SOFTWARE\Jens Kaesler\WireGuard Credential Provider]
"LogLevel"=dword:00000003
"LogPath"="C:\\wgcp_debug.log"
```

The log shows all steps: profiles found, connection status, icon loading, connect/disconnect actions. Reset `LogLevel` to `1` after diagnostics.

---

## 🤝 Contributing

Issues and pull requests are welcome. Areas of particular interest:

- 📦 MSI installer
- 🌐 Localization support
- 🔒 Optional PIN prompt before connecting
- 🔔 Notification after successful connection

---

## 📜 License

[MIT](LICENSE) – do whatever you want with it, but without any warranty.

---

## 🙏 Background

Born out of a practical need to reliably connect domain-joined laptops to the corporate network via WireGuard from home and on the road – without relying on expensive enterprise VPN solutions. The full development history is documented in the [CHANGELOG](CHANGELOG.md).
