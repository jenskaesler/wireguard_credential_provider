# Installer

## Prerequisites

| Tool | Download |
|---|---|
| NSIS 3.x | https://nsis.sourceforge.io/Download |
| nsProcess Plugin | https://nsis.sourceforge.io/NsProcess_plugin |
| SimpleSC Plugin | https://nsis.sourceforge.io/NSIS_Simple_Service_Plugin |

Place the plugins in `C:\Program Files (x86)\NSIS\Plugins\x86-unicode\`.

## Build

1. Visual Studio: build both projects in **Release | x64**
2. Run `build.bat` – copies the binaries and invokes NSIS

The finished installer is written to `installer\Setup_WireGuardCredentialProvider_x64.exe`.

## Included Files

The installer distributes all dependencies itself. No separate deploy step is required:

- `WireGuardCredentialProvider.dll` → `%SystemRoot%\System32\`
- `WireGuardShutdownService.exe` → `%SystemRoot%\System32\`
- Default registry configuration → imported on first install

## Directory Structure

```
installer/
├── build.bat
├── WireGuardCredentialProvider.nsi
├── content/
│   ├── WireGuardCredentialProvider.dll  ← copied by build.bat
│   ├── WireGuardShutdownService.exe     ← copied by build.bat
│   ├── configure.reg                    ← default configuration
│   ├── docs/
│   │   └── LICENSE.rtf
│   └── img/
│       ├── wgcp.ico         ← installer icon (16/32/48px, WireGuard original)
│       ├── wizard.bmp       ← welcome image (164×314px)
│       └── header.bmp       ← header banner (150×57px)
```
