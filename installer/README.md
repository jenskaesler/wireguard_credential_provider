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
| `/LOGLEVEL=n` | REG_SZ | Log-Level (0=off, 1=crit, 2=warn, 3=debug) | `/LOGLEVEL=3` |
| `/HANDSHAKE=n` | REG_SZ | Handshake-Timeout in Sekunden (0=deaktiviert) | `/HANDSHAKE=180` |
| `/THUMBPRINT=hex` | REG_SZ | YubiKey Zertifikat SHA-1 Thumbprint (40 Hex) | `/THUMBPRINT=164F...` |
| `/SMARTCARD=1` | REG_DWORD | SmartcardEnabled aktivieren | `/SMARTCARD=1` |
| `/PINREQUIRED=1` | REG_DWORD | PIN-Abfrage aktivieren | `/PINREQUIRED=1` |
| `/DISCONNECTREMOVE=1` | REG_DWORD | Tunnel trennen wenn YubiKey entfernt | `/DISCONNECTREMOVE=1` |
| `/TILELABEL=text` | REG_SZ | Beschriftung des Pre-Login-Tiles | `/TILELABEL="Firmen-VPN"` |
| `/CONFIGDIR=pfad` | REG_SZ | WireGuard Konfigurationsverzeichnis | `/CONFIGDIR="C:\WG\"` |
| `/UPDATECHECK=n` | REG_DWORD | Update-Prüfung aktivieren/deaktivieren (1/0) | `/UPDATECHECK=0` |

> Parameter werden immer angewendet – auch bei Updates. Nicht angegebene Parameter bleiben unverändert.

---

## Update-Verhalten

Der Installer ist update-sicher:

- **Prozesse**: `WireGuardCPTray.exe` und `WireGuardShutdownService.exe` werden über das
  `nsProcess`-Plugin beendet. Der Installer wartet aktiv bis die Prozesse beendet sind
  (250-ms-Polling, max. 10 Sekunden) bevor neue Binaries kopiert werden. Kein "Datei gesperrt"-Dialog.
- **LogonUI.exe**: wird nicht gekillt (geschützter Systemprozess unter SYSTEM/PPL –
  `taskkill /F` würde hängen). LogonUI lädt die neue DLL automatisch beim nächsten
  Winlogon-Zyklus (Bildschirmsperre, nächster Login).
- **Registry**: Benutzereinstellungen (SmartcardEnabled, Thumbprint, etc.) werden
  bei Updates nicht überschrieben. `HandshakeTimeoutSec` wird auf `180` gesetzt wenn
  der Wert `0` oder leer ist.
- **Silente Parameter** (`/HANDSHAKE=`, `/LOGLEVEL=`, etc.) überschreiben immer –
  auch bei Updates.

---

## YubiKey PIV Einrichtung (`Setup-YubiKey.ps1`)

Das PowerShell-Skript richtet einen YubiKey 5 Series für die Smartcard-Authentifizierung ein.

**Voraussetzungen:**
- YubiKey Manager CLI (`ykman`) installiert und im PATH
- PowerShell als Administrator
- YubiKey 5 Series eingesteckt
- YubiKey Minidriver installiert (für PIV-Treiber-Unterstützung)

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
1. Voraussetzungen prüfen (`ykman` vorhanden, Administrator-Rechte, PIV aktiviert)
2. PIV-Anwendung zurücksetzen
3. Zufälligen 8-stelligen PIN und PUK generieren
4. Benutzername abfragen (AD-Abfrage oder manuell)
5. RSA2048-Schlüsselpaar in Slot 9a generieren
6. Selbstsigniertes Zertifikat erstellen (10 Jahre Gültigkeit)
7. `SmartcardEnabled=1` und `SmartcardCertThumbprint` in Registry schreiben
8. `SmartcardDisconnectOnRemove=1` setzen
9. Setup-Bericht mit PIN/PUK an gewähltem Speicherort ablegen

**Fehlerbehandlung (seit 2026.8.4):**
- Kein abrupter Abbruch bei Fehlern – das Skript kehrt nach Fehlern ins Hauptmenü zurück
- Alle `ykman`-Aufrufe gehen durch `Invoke-Ykman` mit klarer Fehlerausgabe und ykman-Meldung
- Erkannte Fehlersituationen mit Lösungshinweisen:
  - `ykman` nicht gefunden → Download-Link
  - Kein YubiKey erkannt → USB-Hinweis, Treiber-Link
  - PIV nicht vorhanden → Modell-Hinweis
  - PIV deaktiviert → `ykman config usb --enable PIV`
  - Slot 9a leer → Verweis auf Option 1
  - Fehlender Minidriver → Download-Link Yubico Smart Card Drivers

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
| `HandshakeTimeoutSec` | REG_SZ | Tunnel trennen wenn Handshake älter als N Sekunden (`"180"`) |
| `AutoUpdateCheck` | REG_DWORD | Automatisch auf neue GitHub-Releases prüfen (`1`=aktiv, `0`=deaktiviert, Standard: `1`) |

`SmartcardEnabled` und `SmartcardCertThumbprint` werden vom `Setup-YubiKey.ps1` gesetzt.

> **Hinweis:** `LogLevel`, `LogRetentionDays` und `HandshakeTimeoutSec` werden als `REG_SZ` gespeichert.
> Im Regedit einfach die Dezimalzahl als Text eingeben – keine Hex-Konvertierung nötig.

---

## Autostart

Die Tray-App startet automatisch über einen Shortcut in `CommonStartup` mit gesetztem `RunAsAdministrator`-Flag.
Dies stellt sicher dass die App als Administrator läuft ohne UAC-Abfrage (auch bei deaktivierter UAC).

Der Shortcut wird beim Deinstallieren automatisch entfernt.

---

## Netzwerkerkennung (Corporate Network Detection)

Die Tray-App erkennt automatisch ob der PC im Firmennetz ist und trennt den VPN-Tunnel:

**Stufe 1** – Windows NLA: `INetworkListManager` meldet ein `DOMAIN_AUTHENTICATED`-Netz.

**Stufe 2** – Fallback für VMs (Red Hat VirtIO, Hyper-V) wo NLA nur `PRIVATE` meldet:
1. Registry-Check: PC ist Domain-joined (`SYSTEM\...\Tcpip\Parameters\Domain`)
2. Nicht-WireGuard LAN-Adapter mit aktiver IPv4-Adresse vorhanden
3. `DsGetDcName` findet einen Domain Controller im Netzwerk
4. `GetBestRoute` prüft: Route zur DC-IP geht über LAN-Adapter (nicht über WireGuard-Tunnel)
   → verhindert False Positives im Homeoffice wo der DC nur über VPN erreichbar ist

Das Ergebnis ist im Log bei `LogLevel=3` unter `CorpNet:` sichtbar.
