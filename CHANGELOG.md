# Changelog

Alle wesentlichen Änderungen an diesem Projekt werden in dieser Datei dokumentiert.

Das Format basiert auf [Keep a Changelog](https://keepachangelog.com/de/1.1.0/).
Die Versionierung folgt dem Schema `<Jahr>.<Monat>.<Release>`.

---

## [2026.7.5] – 2026-07-24

### Hinzugefügt
- Log-Rotation: `LogRetentionDays` (Standard: 7) – Logs älter als N Tage werden beim `Initialize` automatisch gelöscht
- Log-Pfad unterstützt Datums-Platzhalter `ddMMyyyy` → tägliche Log-Dateien (z.B. `wgcp_24072026.log`)
- `InstallDir` wird vom Installer in die Registry geschrieben und als Basis für den Log-Pfad genutzt
- Installer legt `logs\`-Unterordner im Installationsverzeichnis an
- `configure.reg` in den Installer integriert (`installer/content/`) – wird bei Erstinstallation automatisch importiert
- `_DisconnectAllOnBoot` Deklaration in `WireGuardCredential.h` ergänzt (Compiler-Fehler behoben)

### Geändert
- `LogLevel` Default: `0` → `1` (CRIT) – kritische Fehler werden immer geloggt
- Icons (verbunden/getrennt) auf Original-WireGuard-Logo zurückgerollt – sauber als RGBA composited auf grünem bzw. rotem Hintergrund
- `deploy/`-Verzeichnis aufgelöst: `configure.reg` nach `installer/content/` migriert, `install.bat`/`uninstall.bat` entfernt (werden durch den Installer ersetzt)
- `.gitignore` angepasst

---

## [2026.7.4] – 2026-07-23

### Geändert
- Felddefinitionen (`g_rgFields`, `g_rgFieldStates`) in zentrale Headerdatei `FieldDescriptors.h` ausgelagert – eliminiert doppelte Definitionen in `WireGuardProvider.cpp` und `WireGuardCredential.cpp`
- `_bConnected`, `_bSelected` und `_bStopTimer` als `volatile` markiert – korrekte Sichtbarkeit zwischen Timer-Thread und UI-Thread
- Shutdown-Service nutzt `CREATE_NO_WINDOW` beim Starten von `wireguard.exe` – kein kurzes Aufblitzen eines Konsolenfensters beim Shutdown
- `static_assert` in `FieldDescriptors.h` stellt zur Compile-Zeit sicher dass Feldzahl und `FI_NUM_FIELDS` übereinstimmen

### Behoben
- `_DisconnectAllOnBoot()` entfernt – die Funktion trennte fälschlicherweise aktive Tunnel direkt nach dem Verbinden
- `WireGuardShutdownService` trennte Tunnel beim Dienststart statt nur beim `PRESHUTDOWN`-Event
- Endlosschleife durch `CredentialsChanged` → `SetSelected` → `_UpdateFields` → `CredentialsChanged` behoben: `NotifyStatusChanged()` wird jetzt ausschließlich in `CommandLinkClicked` nach einer echten Verbinden/Trennen-Aktion aufgerufen

---

## [2026.7.3] – 2026-07-23

### Hinzugefügt
- **WireGuardShutdownService**: eigenständiger Windows-Dienst der auf `SERVICE_CONTROL_PRESHUTDOWN` reagiert und beim PC-Shutdown alle aktiven WireGuard-Tunnel sauber trennt
- Preshutdown-Timeout von 30 Sekunden konfiguriert – ausreichend Zeit um alle Tunnel zu beenden
- `install.bat` und `uninstall.bat` installieren/deinstallieren jetzt beide Komponenten (DLL + Service) in einem Schritt
- Manueller Test-Modus: `WireGuardShutdownService.exe /run` trennt alle Tunnel ohne Neustart

### Geändert
- Shutdown-Listener-Fenster von `HWND_MESSAGE` auf sichtbares Top-Level-Fenster (`WS_POPUP`, 0×0 Pixel) umgestellt – `HWND_MESSAGE`-Fenster empfangen `WM_ENDSESSION` nicht zuverlässig

---

## [2026.7.2] – 2026-07-23

### Hinzugefügt
- **Automatischer Status-Refresh** alle 5 Sekunden wenn die Kachel ausgewählt ist (Background-Thread)
- **Verbindungstimer**: `⏱ Verbunden seit HH:MM:SS` – liest Prozess-Startzeit des Tunnel-Services
- **Traffic-Statistiken**: `↑ X MB ↓ Y MB` via `wg.exe show <profil> transfer` – wird nur bei aktiver Verbindung angezeigt
- **Farbige Icons**: zwei separate BMP-Ressourcen (`wireguard_connected.bmp` grüner Hintergrund, `wireguard_disconnected.bmp` roter Hintergrund) direkt in die DLL eingebettet
- Icons als Projekt-Ressourcen integriert – keine externen Dateien zur Laufzeit erforderlich
- `ICredentialProviderEvents::CredentialsChanged` nach Verbinden/Trennen um Icon-Reload zu erzwingen
- `wg.exe`-Pfad über Registry-Wert `WgExePath` konfigurierbar

### Geändert
- Trennen-Befehl korrigiert: `/removetunnelservice` → `/uninstalltunnelservice` (korrekter WireGuard-Befehlsname)
- Anführungszeichen beim Tunnelnamen für `/uninstalltunnelservice` entfernt – WireGuard erwartet den Namen ohne Quotes
- Nach Verbinden/Trennen aktives Warten auf Service-Statuswechsel statt festem `Sleep(2000)`
- `GetModuleHandleExW`-Anker von Memberfunktionszeiger auf statische Hilfsfunktion umgestellt

---

## [2026.7.1] – 2026-07-23

### Hinzugefügt
- **Profil-Dropdown (ComboBox)**: listet alle `.conf.dpapi`-Konfigurationen aus dem WireGuard-Konfigurationsverzeichnis
- **Automatisches Standardprofil**: sucht nach einer Konfigurationsdatei die dem Computernamen entspricht (z.B. `LT260430.conf.dpapi`)
- **Verbinden/Trennen-Button**: `▶ Verbinden` / `⏏ Trennen` – ausgegraut wenn kein Profil verfügbar
- **Statusanzeige**: `● Verbunden` / `○ Getrennt` als Textfeld unterhalb des Labels
- Tunnel verbinden via `wireguard.exe /installtunnelservice <pfad-zur-config>`
- Tunnel trennen via `wireguard.exe /uninstalltunnelservice <tunnelname>`
- Verbindungsstatus-Erkennung über Windows-Service `WireGuardTunnel$<Profilname>`
- Log-Level und Log-Pfad über Registry konfigurierbar (`LogLevel` DWORD: 0=aus, 1=CRIT, 2=WARN, 3=DEBUG)
- Konfigurierbare Icons für verbunden/getrennt Zustand (`IconConnected`, `IconDisconnected`)

### Geändert
- Kachel-Klick löst keine externe Programminstanz mehr aus – Verbinden/Trennen erfolgt direkt über WireGuard-Service-Mechanismus
- `_UpdateFields` aktualisiert Statustext, Traffic und Button-Beschriftung ohne Neuenumeration

---

## [2026.7.0] – 2026-07-23

### Hinzugefügt
- **WireGuard Credential Provider** als Windows-DLL (`ICredentialProvider` + `ICredentialProviderCredential`)
- Kachel erscheint auf dem Windows-Anmeldebildschirm und beim Sperrbildschirm (Logon + Unlock)
- Konfiguration vollständig über Registry (`HKLM\SOFTWARE\WireGuardCredentialProvider`)
- Konfigurierbarer Pfad zur `wireguard.exe` (`ExePath`)
- Konfigurierbarer Kacheltext (`TileLabel`)
- Konfigurierbares Kachel-Icon (`IconPath`, 128×128 px, 24bpp BMP)
- Dateibasiertes Logging mit Zeitstempel (`LogPath`, `LogLevel`)
- `regsvr32`-kompatible Registrierung/Deregistrierung (`DllRegisterServer`/`DllUnregisterServer`)
- Installations-Skripte: `install.bat`, `uninstall.bat`, `configure.reg`
- Visual Studio 2022/2026 Projektdateien (`.sln`, `.vcxproj`)

### Technisches Fundament
- COM In-Process-Server mit `IClassFactory`
- Thread-sicherer Referenzzähler via `InterlockedIncrement`/`InterlockedDecrement`
- Unicode durchgängig (`WCHAR`, `W`-Suffix-APIs)
- Alle Strings via `StringCch*`-Familie (keine unsicheren `strcpy`/`sprintf`)
- Ressourcen via `CoTaskMemAlloc`/`CoTaskMemFree` entsprechend COM-Konvention
