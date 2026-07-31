# WireGuard Credential Provider – Installer

## Inhalt

| Datei | Beschreibung |
|---|---|
| `Setup_WireGuardCredentialProvider_x64.exe` | Installer (wird von `build.bat` erzeugt) |
| `WireGuardCredentialProvider.nsi` | NSIS-Skript |
| `Setup-YubiKey.ps1` | YubiKey PIV Einrichtungs- und Registrierungs-Tool |
| `build.bat` | Kopiert Binaries und kompiliert den Installer |
| `content/` | Installer-Assets (Icons, Registrierungsdateien) |

---

## Silent-Installation

```cmd
:: Minimal (nur CP + Tray)
Setup_WireGuardCredentialProvider_x64.exe /S

:: Vollständig (CP + Tray + alle YubiKey-Tools)
Setup_WireGuardCredentialProvider_x64.exe /S /FULL

:: Individuelle YubiKey-Komponenten
Setup_WireGuardCredentialProvider_x64.exe /S /YKAUTH /YKMINI /YKMGRCLI

:: Silent-Deinstallation
Uninstall.exe /S
```

---

## YubiKey PIV Einrichtung (`Setup-YubiKey.ps1`)

Das PowerShell-Skript richtet einen YubiKey 5 Series für die Smartcard-Authentifizierung ein.

**Voraussetzungen:**
- YubiKey Manager CLI (`ykman`) installiert
- PowerShell als Administrator
- YubiKey 5 Series eingesteckt

**Ausführen:**
```powershell
Set-ExecutionPolicy Bypass -Scope Process
.\Setup-YubiKey.ps1
```

**Menü:**

| Option | Beschreibung |
|---|---|
| **1 – YubiKey initialisieren** | Vollständiger PIV-Reset, Schlüssel + Zertifikat generieren, Registry schreiben |
| **2 – Bestehenden YubiKey registrieren** | Thumbprint vom gesteckten YubiKey lesen, in Registry schreiben |
| **3 – Setup-Bericht exportieren** | Bericht neu erstellen (YubiKey muss gesteckt sein) |

**Was das Skript tut (Option 1):**
1. PIV-Anwendung zurücksetzen
2. Zufälligen 8-stelligen PIN und PUK generieren
3. Benutzername abfragen (AD-Abfrage oder manuell)
4. RSA2048-Schlüsselpaar in Slot 9a generieren
5. Selbstsigniertes Zertifikat erstellen (10 Jahre Gültigkeit)
6. `SmartcardEnabled=1` und `SmartcardCertThumbprint` in Registry schreiben
7. `SmartcardDisconnectOnRemove=1` setzen
8. Setup-Bericht mit PIN/PUK an gewähltem Speicherort ablegen

> **Unterstützte YubiKey-Modelle:** 5 NFC, 5C, 5Ci, 5 Nano, 5C NFC, 5C Nano  
> **Nicht unterstützt:** YubiKey Bio, Security Key, YubiKey 4 Series

---

## Registry-Schlüssel

`HKLM\SOFTWARE\Jens Kaesler\WireGuard Credential Provider`

Der Installer setzt folgende Werte:

| Wert | Typ | Beschreibung |
|---|---|---|
| `ExePath` | REG_SZ | Pfad zu `wireguard.exe` |
| `WgExePath` | REG_SZ | Pfad zu `wg.exe` |
| `ConfigDir` | REG_SZ | Konfigurationsverzeichnis |
| `InstallDir` | REG_SZ | Installationsverzeichnis |
| `LogLevel` | REG_DWORD | Log-Level (1=CRIT, 3=DEBUG) |

`SmartcardEnabled` und `SmartcardCertThumbprint` werden vom `Setup-YubiKey.ps1` gesetzt.
