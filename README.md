# 🔐 WireGuard Credential Provider

> Verbinde dein WireGuard-VPN direkt vom Windows-Anmeldebildschirm aus – noch bevor du dich einloggst.

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows&logoColor=white)](https://www.microsoft.com/windows)
[![Language](https://img.shields.io/badge/Language-C%2B%2B17-00599C?logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![License](https://img.shields.io/badge/Lizenz-MIT-green)](LICENSE)
[![Version](https://img.shields.io/badge/Version-2026.7.4-blue)](CHANGELOG.md)
[![WireGuard](https://img.shields.io/badge/WireGuard-Windows-88171A?logo=wireguard&logoColor=white)](https://www.wireguard.com/install/)

---

## 🤔 Warum gibt es das?

Domain-joined Windows-Clients stehen vor einem klassischen Henne-Ei-Problem: Der Benutzer muss sich anmelden, um eine VPN-Verbindung herzustellen – aber die VPN-Verbindung wird benötigt, damit der Domänencontroller für die Anmeldung erreichbar ist.

Viele kommerzielle VPN-Clients lösen das mit einer "Pre-Logon"-Funktion. **WireGuard für Windows bietet diese Funktion nicht** – bis jetzt.

Der **WireGuard Credential Provider** ergänzt den Windows-Anmeldebildschirm um eine eigene Kachel, über die Tunnel direkt vor der Anmeldung gestartet und gestoppt werden können.

---

## ✨ Features

- 🔌 **Pre-Logon VPN** – Tunnel verbinden und trennen bevor Windows-Anmeldung
- 📋 **Profil-Auswahl** – Dropdown mit allen vorhandenen `.conf.dpapi`-Konfigurationen
- 🎯 **Automatisches Standardprofil** – erkennt eine Konfiguration die dem Computernamen entspricht (z.B. `LT260430.conf.dpapi`)
- 🟢🔴 **Farbige Status-Icons** – grünes Icon bei aktiver Verbindung, rotes bei getrennter
- ⏱️ **Live-Verbindungszeit** – zeigt wie lange der Tunnel bereits aktiv ist
- 📊 **Traffic-Statistiken** – Up-/Download-Durchsatz in Echtzeit (via `wg.exe`)
- 🔄 **Auto-Refresh** – Status und Traffic werden alle 5 Sekunden automatisch aktualisiert
- 🛑 **Sauberer Shutdown** – separater Windows-Dienst trennt alle Tunnel beim Herunterfahren
- ⚙️ **Vollständig konfigurierbar** – alle Pfade und Texte über die Windows-Registry
- 📝 **Strukturiertes Logging** – konfigurierbare Log-Ebenen (CRIT / WARN / DEBUG)
- 🏢 **GPO-fähig** – Registry-Konfiguration lässt sich per Group Policy verteilen

---

## 📋 Voraussetzungen

| Komponente | Version |
|---|---|
| Windows | 10 oder 11 (x64) |
| WireGuard für Windows | [aktuelle Version](https://www.wireguard.com/install/) |
| Visual Studio | 2022 oder 2026 |
| Windows SDK | 10.0 |
| C++ Workload | Desktop-Entwicklung mit C++ |

---

## 🏗️ Projektstruktur

```
WireGuardCredentialProvider/
│
├── src/
│   ├── helpers.h                       – Registry-Helfer, Logging, WireGuard-Funktionen
│   ├── FieldDescriptors.h              – Zentrale Felddefinitionen (kein Duplikat)
│   ├── WireGuardProvider.h/.cpp        – ICredentialProvider (Rahmen, Enumeration)
│   ├── WireGuardCredential.h/.cpp      – ICredentialProviderCredential (Kachel, Logik)
│   ├── dll.cpp                         – DllMain, DllGetClassObject, regsvr32-Exports
│   └── WireGuardCredentialProvider.def – DLL-Exportdefinitionen
│
├── resources/
│   ├── resource.h                      – Ressourcen-IDs
│   ├── WireGuardCredentialProvider.rc  – Ressourcen-Skript (eingebettete Icons)
│   ├── wireguard_connected.bmp         – Icon bei aktiver Verbindung (128×128, grün)
│   └── wireguard_disconnected.bmp      – Icon bei getrennter Verbindung (128×128, rot)
│
├── shutdown-service/
│   ├── WireGuardShutdownService.vcxproj
│   └── src/
│       └── WireGuardShutdownService.cpp – Windows-Dienst für sauberen Shutdown
│
├── deploy/
│   ├── install.bat                     – Installations-Skript (als Admin ausführen)
│   ├── uninstall.bat                   – Deinstallations-Skript
│   └── configure.reg                   – Registry-Konfiguration importieren
│
├── WireGuardCredentialProvider.sln
├── WireGuardCredentialProvider.vcxproj
├── CHANGELOG.md
├── LICENSE
└── README.md
```

---

## ⚡ Schnellstart

### 1. Repository klonen

```cmd
git clone https://github.com/jenskaesler/wireguard-credential-provider.git
cd wireguard-credential-provider
```

### 2. Bauen

Visual Studio öffnen → `WireGuardCredentialProvider.sln` → **Release | x64** → **Strg+Shift+B**

Beide Projekte werden gebaut:
- `x64\Release\WireGuardCredentialProvider.dll`
- `shutdown-service\x64\Release\WireGuardShutdownService.exe`

### 3. Ausgaben vorbereiten

```cmd
copy x64\Release\WireGuardCredentialProvider.dll deploy\
copy shutdown-service\x64\Release\WireGuardShutdownService.exe deploy\
```

### 4. Installieren

```cmd
:: Als Administrator:
deploy\install.bat
```

Das Skript übernimmt:
- Kopiert die DLL nach `%SystemRoot%\System32\`
- Registriert den Credential Provider via `regsvr32`
- Importiert die Standardkonfiguration aus `configure.reg`
- Installiert und startet den `WireGuardShutdownHelper`-Dienst

### 5. Testen

Bildschirm sperren (`Win+L`) → WireGuard-Kachel erscheint neben den Benutzer-Kacheln.

---

## ⚙️ Konfiguration

Alle Einstellungen unter `HKEY_LOCAL_MACHINE\SOFTWARE\WireGuardCredentialProvider`:

| Wert | Typ | Beschreibung | Standard |
|---|---|---|---|
| `ExePath` | REG_SZ | Pfad zur `wireguard.exe` | `C:\Program Files\WireGuard\wireguard.exe` |
| `WgExePath` | REG_SZ | Pfad zur `wg.exe` (Traffic-Stats) | `C:\Program Files\WireGuard\wg.exe` |
| `TileLabel` | REG_SZ | Überschrift auf der Kachel | `WireGuard VPN` |
| `IconConnected` | REG_SZ | Eigenes Icon (verbunden, 128×128 BMP) | *(eingebettete Ressource)* |
| `IconDisconnected` | REG_SZ | Eigenes Icon (getrennt, 128×128 BMP) | *(eingebettete Ressource)* |
| `LogPath` | REG_SZ | Pfad zur Logdatei | `C:\wgcp_debug.log` |
| `LogLevel` | REG_DWORD | Log-Ebene | `0` (aus) |

**Log-Ebenen:**
- `0` – Logging deaktiviert (Produktion)
- `1` – Nur kritische Fehler
- `2` – Warnungen und Fehler
- `3` – Alles (Diagnose)

### Konfiguration per GPO verteilen

Die Registry-Werte lassen sich per **Group Policy Preferences (GPP)** auf alle Domain-Clients ausrollen. Die DLL und den Shutdown-Service verteilt ein Logon-Script oder Software-Deployment.

### Eigene Icons verwenden

Icons müssen **128×128 Pixel, 24bpp, unkomprimiertes BMP** sein. Sind `IconConnected` und `IconDisconnected` in der Registry gesetzt, werden diese statt der eingebetteten Ressourcen verwendet.

---

## 🛑 Shutdown-Verhalten

Der **WireGuard Shutdown Helper** (`WireGuardShutdownService.exe`) läuft als Windows-Dienst (Starttyp: Automatisch). Er registriert sich für `SERVICE_CONTROL_PRESHUTDOWN` – Windows benachrichtigt ihn vor dem eigentlichen Shutdown, damit alle aktiven Tunnel sauber getrennt werden können (Timeout: 30 Sekunden).

```cmd
:: Status prüfen
sc query WireGuardShutdownHelper

:: Manueller Test – trennt alle aktiven Tunnel sofort
WireGuardShutdownService.exe /run

:: Deinstallieren
WireGuardShutdownService.exe /uninstall
```

---

## 🔧 Technische Details

### Architektur

Der Credential Provider ist eine **COM In-Process-DLL** die von `LogonUI.exe` auf dem `Winsta0\Winlogon`-Desktop geladen wird. Er implementiert:

- `ICredentialProvider` – Registrierung und Enumeration der Kacheln
- `ICredentialProviderCredential` – Darstellung und Interaktion der Kachel

Die Kachel gibt keine Credential-Serialisierung zurück (`CPGSR_NO_CREDENTIAL_NOT_FINISHED`) – sie dient ausschließlich zur VPN-Steuerung und lässt den normalen Windows-Login unberührt.

### WireGuard-Integration

| Aktion | Befehl |
|---|---|
| Tunnel verbinden | `wireguard.exe /installtunnelservice "C:\...\Profil.conf.dpapi"` |
| Tunnel trennen | `wireguard.exe /uninstalltunnelservice Profilname` |
| Verbindungsstatus | Service `WireGuardTunnel$Profilname` abfragen |
| Traffic-Stats | `wg.exe show Profilname transfer` |

### Konfigurationsverzeichnis

WireGuard speichert verschlüsselte Konfigurationen unter:
```
C:\Program Files\WireGuard\Data\Configurations\*.conf.dpapi
```
Der Credential Provider liest die Dateinamen (ohne `.conf.dpapi`) für das Profil-Dropdown. Der Inhalt der verschlüsselten Dateien wird nicht gelesen – `wireguard.exe` übernimmt die Entschlüsselung intern.

### Sicherheitshinweis

Credential Provider und Shutdown-Service laufen als **SYSTEM**. Der Registry-Schlüssel `HKLM\SOFTWARE\WireGuardCredentialProvider` ist standardmäßig nur für Administratoren beschreibbar – stelle sicher dass dies in deiner Umgebung so bleibt.

---

## 🔑 Smartcard / YubiKey PIV (Zweiter Faktor)

Der Credential Provider unterstützt optional eine **Smartcard- oder YubiKey-Authentifizierung** als zweiten Faktor vor dem VPN-Verbindungsaufbau. Der YubiKey wird dabei im **PIV-Modus** (Personal Identity Verification) betrieben – Windows sieht ihn wie eine klassische Smartcard.

### Voraussetzungen

- YubiKey mit PIV-Zertifikat (Einrichtung via [YubiKey Manager](https://www.yubico.com/support/download/yubikey-manager/))
- Windows Smartcard-Treiber (bereits enthalten in Windows 10/11)

### Konfiguration

Alle Smartcard-Einstellungen unter `HKEY_LOCAL_MACHINE\SOFTWARE\WireGuardCredentialProvider`:

| Wert | Typ | Beschreibung | Standard |
|---|---|---|---|
| `SmartcardEnabled` | DWORD | Smartcard-Authentifizierung aktivieren | `0` (aus) |
| `SmartcardPinRequired` | DWORD | PIN-Eingabe erforderlich | `1` (ja) |
| `SmartcardPinMinLength` | DWORD | Minimale PIN-Länge | `4` |
| `SmartcardPinMaxAttempts` | DWORD | Max. Fehlversuche bis Warnung | `3` |
| `SmartcardTimeout` | DWORD | Sekunden warten auf Karte | `10` |
| `SmartcardConnectOnInsert` | DWORD | Tunnel automatisch verbinden wenn YubiKey eingesteckt | `0` (nein) |
| `SmartcardDisconnectOnRemove` | DWORD | Tunnel automatisch trennen wenn YubiKey entfernt | `0` (nein) |
| `SmartcardReaderName` | REG_SZ | Bestimmten Reader erzwingen (leer = erster verfügbarer) | `""` |
| `SmartcardCertThumbprint` | REG_SZ | SHA1-Thumbprint des erwarteten Zertifikats (leer = beliebig) | `""` |

### Aktivierung

```reg
[HKEY_LOCAL_MACHINE\SOFTWARE\WireGuardCredentialProvider]
"SmartcardEnabled"=dword:00000001
"SmartcardPinRequired"=dword:00000001
"SmartcardConnectOnInsert"=dword:00000001
"SmartcardDisconnectOnRemove"=dword:00000001
```

### Ablauf mit aktivierter Smartcard

1. Loginscreen erscheint – Kachel zeigt `🔑 Bitte YubiKey / Smartcard einstecken...`
2. YubiKey einstecken → `✅ Smartcard erkannt`
3. PIN eingeben (wenn `SmartcardPinRequired=1`)
4. **Verbinden** drücken → PIV VERIFY APDU wird gesendet
5. Bei Erfolg: Tunnel verbindet sich
6. YubiKey entfernen → Tunnel trennt automatisch (wenn `SmartcardDisconnectOnRemove=1`)

### Thumbprint ermitteln

```powershell
# PowerShell – Thumbprint des PIV-Zertifikats anzeigen
Get-ChildItem Cert:\LocalMachine\My | Where-Object { $_.Subject -like "*YubiKey*" } | Select-Object Thumbprint, Subject
```


---

## 🐛 Fehlersuche

`LogLevel` auf `3` setzen:

```reg
[HKEY_LOCAL_MACHINE\SOFTWARE\WireGuardCredentialProvider]
"LogLevel"=dword:00000003
"LogPath"="C:\\wgcp_debug.log"
```

Das Log zeigt alle Schritte: Profile gefunden, Verbindungsstatus, Icon-Laden, Verbinden/Trennen-Aktionen. Nach der Diagnose `LogLevel` wieder auf `0` setzen.

---

## 🤝 Mitmachen

Issues und Pull Requests sind willkommen. Besonders interessant:

- 🌐 Englische Lokalisierung
- 📦 MSI-Installer
- 🔒 Optionale PIN-Abfrage vor dem Verbinden
- 🔔 Benachrichtigung nach erfolgreicher Verbindung

---

## 📜 Lizenz

[MIT](LICENSE) – mach damit, was du willst, aber ohne Garantie.

---

## 🙏 Entstehungsgeschichte

Entstanden aus dem praktischen Bedarf, Domain-joined Laptops auch im Homeoffice und unterwegs sauber per WireGuard in das Firmennetz einzubinden – ohne auf teure Enterprise-VPN-Lösungen angewiesen zu sein. Die gesamte Entwicklungsgeschichte ist im [CHANGELOG](CHANGELOG.md) dokumentiert.
