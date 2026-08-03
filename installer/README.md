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

:: Mit Registry-Parametern (alle optional, kombinierbar)
Setup_WireGuardCredentialProvider_x64.exe /S /SMARTCARD=1 /THUMBPRINT=164F6689... /DISCONNECTREMOVE=1
Setup_WireGuardCredentialProvider_x64.exe /S /LOGLEVEL=3
Setup_WireGuardCredentialProvider_x64.exe /S /HANDSHAKE=180 /TILELABEL="Firmen-VPN"
Setup_WireGuardCredentialProvider_x64.exe /S /CONFIGDIR="C:\WG\Configs\"

:: Vollständig (CP + Tray + alle YubiKey-Tools)
Setup_WireGuardCredentialProvider_x64.exe /S /FULL

:: Individuelle YubiKey-Komponenten
Setup_WireGuardCredentialProvider_x64.exe /S /YKAUTH /YKMINI /YKMGRCLI

:: Silent-Deinstallation
Uninstall.exe /S
```

---

## Silent-Parameter

| Parameter | Typ | Beschreibung | Beispiel |
|---|---|---|---|
| `/LOGLEVEL=n` | DWORD | Log-Level (0=off, 1=crit, 2=warn, 3=debug) | `/LOGLEVEL=3` |
| `/HANDSHAKE=n` | DWORD | Handshake-Timeout in Sekunden (0=deaktiviert) | `/HANDSHAKE=180` |
| `/THUMBPRINT=hex` | REG_SZ | YubiKey Zertifikat SHA-1 Thumbprint (40 Hex) | `/THUMBPRINT=164F...` |
| `/SMARTCARD=1` | DWORD | SmartcardEnabled aktivieren | `/SMARTCARD=1` |
| `/PINREQUIRED=1` | DWORD | PIN-Abfrage aktivieren | `/PINREQUIRED=1` |
| `/DISCONNECTREMOVE=1` | DWORD | Tunnel trennen wenn YubiKey entfernt | `/DISCONNECTREMOVE=1` |
| `/TILELABEL=text` | REG_SZ | Beschriftung des Pre-Login-Tiles | `/TILELABEL="Firmen-VPN"` |
| `/CONFIGDIR=pfad` | REG_SZ | WireGuard Konfigurationsverzeichnis | `/CONFIGDIR="C:\WG\"` |

> Parameter werden immer angewendet – auch bei Updates. Nicht angegebene Parameter bleiben unverändert.

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
| `LogLevel` | REG_SZ | Log-Level als Dezimalzahl (`"1"`=CRIT, `"3"`=DEBUG) |
| `LogRetentionDays` | REG_SZ | Log-Dateien älter als N Tage löschen (`"7"`) |
| `HandshakeTimeoutSec` | REG_SZ | Tunnel trennen wenn Handshake älter als N Sekunden (`"0"`=aus) |

`SmartcardEnabled` und `SmartcardCertThumbprint` werden vom `Setup-YubiKey.ps1` gesetzt.

> **Hinweis:** `LogLevel`, `LogRetentionDays` und `HandshakeTimeoutSec` werden als `REG_SZ` gespeichert.
> Im Regedit einfach die Dezimalzahl als Text eingeben – keine Hex-Konvertierung nötig.

---

## Autostart

Die Tray-App startet automatisch über einen Shortcut in `CommonStartup` mit gesetztem `RunAsAdministrator`-Flag.
Dies stellt sicher dass die App als Administrator läuft ohne UAC-Abfrage (auch bei deaktivierter UAC).

Der Shortcut wird beim Deinstallieren automatisch entfernt.
