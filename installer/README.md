# Installer

## Voraussetzungen

| Tool | Download |
|---|---|
| NSIS 3.x | https://nsis.sourceforge.io/Download |
| nsProcess Plugin | https://nsis.sourceforge.io/NsProcess_plugin |
| SimpleSC Plugin | https://nsis.sourceforge.io/NSIS_Simple_Service_Plugin |

Plugins in `C:\Program Files (x86)\NSIS\Plugins\x86-unicode\` ablegen.

## Build

1. Visual Studio: beide Projekte in **Release | x64** bauen
2. `build.bat` ausführen – kopiert die Binaries und ruft NSIS auf

Der fertige Installer liegt als `Setup_WireGuardCredentialProvider_x64.exe` im `installer\`-Verzeichnis.

## Enthaltene Dateien

Der Installer verteilt alle Abhängigkeiten selbst. Kein separater deploy-Schritt nötig:

- `WireGuardCredentialProvider.dll` → `%SystemRoot%\System32\`
- `WireGuardShutdownService.exe` → `%SystemRoot%\System32\`  
- `configure.reg` → wird bei Erstinstallation importiert

## Verzeichnisstruktur

```
installer/
├── build.bat
├── WireGuardCredentialProvider.nsi
├── content/
│   ├── WireGuardCredentialProvider.dll  ← von build.bat kopiert
│   ├── WireGuardShutdownService.exe     ← von build.bat kopiert
│   ├── configure.reg                    ← Standard-Konfiguration
│   ├── docs/
│   │   └── LICENSE.rtf
│   └── img/
│       ├── wgcp.ico         ← Installer-Icon (16/32/48px, WireGuard-Original)
│       ├── wizard.bmp       ← Willkommens-Bild (164×314px)
│       └── header.bmp       ← Header-Banner (150×57px)
```
