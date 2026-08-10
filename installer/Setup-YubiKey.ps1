#Requires -Version 5.1
<#
.SYNOPSIS
    YubiKey PIV Setup fuer WireGuard Credential Provider
.DESCRIPTION
    Menue-gesteuertes Tool fuer YubiKey PIV Verwaltung:
    1. YubiKey initialisieren (PIV reset, PIN, Zertifikat, Registry)
    2. Thumbprint verschluesseln (aus Textdatei in Registry schreiben)
    3. Setup-Bericht exportieren (Bericht aus Registry-Daten neu erstellen)
.NOTES
    Benoetigt: ykman (YubiKey Manager CLI)
    Ausfuehren als: Administrator
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Hilfsfunktionen
# ---------------------------------------------------------------------------
function Write-Step { param($msg) Write-Host "`n[>] $msg" -ForegroundColor Cyan }
function Write-Ok   { param($msg) Write-Host "    [OK] $msg" -ForegroundColor Green }
function Write-Warn { param($msg) Write-Host "    [!!] $msg" -ForegroundColor Yellow }
function Write-Fail { param($msg) Write-Host "`n[FEHLER] $msg" -ForegroundColor Red; exit 1 }
function Write-Info { param($msg) Write-Host "    [i]  $msg" -ForegroundColor DarkGray }

function Write-Header {
    Clear-Host
    Write-Host "============================================================" -ForegroundColor DarkCyan
    Write-Host "  WireGuard Credential Provider - YubiKey PIV Tool" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor DarkCyan
}

# ---------------------------------------------------------------------------
# Invoke-Ykman – robuster ykman-Wrapper
# Führt ykman aus, fängt alle Ausgaben ab und gibt bei Fehler eine
# klare Meldung aus statt rohe ykman-Fehlertexte durchzuleiten.
# Rückgabe: [string] stdout-Ausgabe, oder $null bei Fehler
# ---------------------------------------------------------------------------
function Invoke-Ykman {
    param(
        [string[]]$Arguments,
        [string]$ErrorContext = "ykman-Befehl"
    )
    try {
        $output = & ykman @Arguments 2>&1 | Out-String
        if ($LASTEXITCODE -ne 0) {
            # ykman selbst gibt oft hilfreiche Fehlertexte aus – diese anzeigen
            $cleanOutput = ($output -replace '(?m)^\s+$','').Trim()
            Write-Host ""
            Write-Host "    [FEHLER] $ErrorContext fehlgeschlagen." -ForegroundColor Red
            if ($cleanOutput) {
                Write-Host "    Ykman-Meldung:" -ForegroundColor DarkGray
                foreach ($line in ($cleanOutput -split "`n")) {
                    $line = $line.TrimEnd()
                    if ($line) { Write-Host "      $line" -ForegroundColor DarkGray }
                }
            }
            return $null
        }
        return $output
    } catch {
        Write-Host ""
        Write-Host "    [FEHLER] Unerwarteter Fehler beim Ausfuehren von ykman: $_" -ForegroundColor Red
        return $null
    }
}

# ---------------------------------------------------------------------------
# Get-YkmanVersion – ykman-Version lesen ohne Pipeline-Probleme
# ---------------------------------------------------------------------------
function Get-YkmanVersion {
    try {
        $v = & ykman --version 2>&1 | Out-String
        return $v.Trim()
    } catch {
        return "(Version nicht lesbar)"
    }
}

# ---------------------------------------------------------------------------
# Test-Prerequisites – zentrale Voraussetzungsprüfung für alle Optionen
# Prüft: Administrator-Rechte, ykman vorhanden
# Gibt $true zurück wenn alle Voraussetzungen erfüllt sind.
# ---------------------------------------------------------------------------
function Test-Prerequisites {
    $ok = $true

    # Administrator-Rechte
    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) {
        Write-Host ""
        Write-Host "  [FEHLER] Dieses Skript muss als Administrator ausgefuehrt werden." -ForegroundColor Red
        Write-Host "  Bitte das Skript per Rechtsklick > 'Als Administrator ausfuehren' starten." -ForegroundColor Yellow
        $ok = $false
    }

    # ykman vorhanden
    if (-not (Get-Command ykman -ErrorAction SilentlyContinue)) {
        Write-Host ""
        Write-Host "  [FEHLER] ykman (YubiKey Manager CLI) wurde nicht gefunden." -ForegroundColor Red
        Write-Host ""
        Write-Host "  Loesung:" -ForegroundColor Yellow
        Write-Host "  1. Installieren Sie den YubiKey Manager CLI:" -ForegroundColor White
        Write-Host "     https://developers.yubico.com/yubikey-manager/" -ForegroundColor Cyan
        Write-Host "  2. Oder starten Sie diesen PC neu nach einer Neuinstallation" -ForegroundColor White
        Write-Host "     (PATH-Variable wird erst nach Neustart aktualisiert)." -ForegroundColor White
        $ok = $false
    }

    return $ok
}

# ---------------------------------------------------------------------------
# Get-YubiKeyInfo – YubiKey-Informationen lesen mit klarer Fehlerdiagnose
# Gibt Hashtable mit Serial, Model zurueck, oder $null bei Fehler.
# ---------------------------------------------------------------------------
function Get-YubiKeyInfo {
    $ykRaw = Invoke-Ykman -Arguments @('info') -ErrorContext "YubiKey-Info lesen"

    if ($null -eq $ykRaw) {
        Write-Host ""
        Write-Host "  Moegliche Ursachen:" -ForegroundColor Yellow
        Write-Host "  - Kein YubiKey eingesteckt" -ForegroundColor White
        Write-Host "  - YubiKey wird von einer anderen Anwendung verwendet (z.B. Browser)" -ForegroundColor White
        Write-Host "  - USB-Verbindung unterbrochen (YubiKey abstecken und neu einstecken)" -ForegroundColor White
        Write-Host "  - Fehlender oder veralteter YubiKey-Treiber" -ForegroundColor White
        Write-Host ""
        Write-Host "  Treiber-Loesung:" -ForegroundColor Yellow
        Write-Host "  Installieren Sie den YubiKey Minidriver:" -ForegroundColor White
        Write-Host "  https://www.yubico.com/support/download/smart-card-drivers-tools/" -ForegroundColor Cyan
        return $null
    }

    # Prüfen ob ykman erfolgreich war aber keinen YubiKey gefunden hat
    # (kann auf manchen Systemen passieren wenn der USB-Treiber fehlt)
    if ($ykRaw -match 'No YubiKey' -or $ykRaw -match 'ERROR' -or $ykRaw -match 'No device') {
        Write-Host ""
        Write-Host "  [FEHLER] Kein YubiKey erkannt." -ForegroundColor Red
        Write-Host ""
        Write-Host "  ykman meldet: $($ykRaw.Trim())" -ForegroundColor DarkGray
        Write-Host ""
        Write-Host "  Loesung:" -ForegroundColor Yellow
        Write-Host "  1. YubiKey einstecken und erneut versuchen" -ForegroundColor White
        Write-Host "  2. Bei 'No module' oder Treiberfehler: YubiKey Minidriver installieren" -ForegroundColor White
        Write-Host "     https://www.yubico.com/support/download/smart-card-drivers-tools/" -ForegroundColor Cyan
        return $null
    }

    # Seriennummer und Modell extrahieren – mit robuster Null-Prüfung
    $serialMatch = $ykRaw | Select-String 'Serial number:\s*(\d+)'
    $modelMatch  = $ykRaw | Select-String 'Device type:\s*(.+)'

    $serial = if ($serialMatch -and $serialMatch.Matches.Count -gt 0 -and
                   $serialMatch.Matches[0].Groups.Count -gt 1) {
        $serialMatch.Matches[0].Groups[1].Value.Trim()
    } else { '(unbekannt)' }

    $model = if ($modelMatch -and $modelMatch.Matches.Count -gt 0 -and
                  $modelMatch.Matches[0].Groups.Count -gt 1) {
        $modelMatch.Matches[0].Groups[1].Value.Trim()
    } else { '(unbekannt)' }

    return @{ Serial = $serial; Model = $model; RawInfo = $ykRaw }
}

# ---------------------------------------------------------------------------
# Test-PivSupport – prüft ob PIV vorhanden UND aktiviert ist
# ---------------------------------------------------------------------------
function Test-PivSupport {
    param([hashtable]$YkInfo)

    $raw = $YkInfo.RawInfo

    # Prüfen ob PIV in der Ausgabe erwähnt wird
    if ($raw -notmatch 'PIV') {
        Write-Host ""
        Write-Host "  [FEHLER] Dieser YubiKey meldet keine PIV-Unterstuetzung." -ForegroundColor Red
        Write-Host ""
        Write-Host "  ykman-Ausgabe (Anwendungen):" -ForegroundColor DarkGray
        # Nur den Abschnitt mit den Anwendungen ausgeben
        $raw -split "`n" | Where-Object { $_ -match 'Applications|PIV|FIDO|OTP|OATH|OpenPGP' } |
            ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
        Write-Host ""
        Write-Host "  Hinweis:" -ForegroundColor Yellow
        Write-Host "  - YubiKey 5-Serie unterstuetzt PIV. YubiKey Bio und Security Key NICHT." -ForegroundColor White
        Write-Host "  - Vergleich unter: https://www.yubico.com/store/compare/" -ForegroundColor Cyan
        return $false
    }

    # Prüfen ob PIV deaktiviert ist (manche YubiKeys haben PIV disabled)
    if ($raw -match 'PIV\s*\n?\s*Enabled:\s*False' -or $raw -match 'PIV.*disabled') {
        Write-Host ""
        Write-Host "  [FEHLER] PIV ist auf diesem YubiKey deaktiviert." -ForegroundColor Red
        Write-Host ""
        Write-Host "  Loesung:" -ForegroundColor Yellow
        Write-Host "  PIV aktivieren mit: ykman config usb --enable PIV" -ForegroundColor Cyan
        Write-Host "  Danach den YubiKey abstecken und neu einstecken." -ForegroundColor White
        return $false
    }

    return $true
}

# ---------------------------------------------------------------------------
# Get-PivCertificate – Zertifikat aus Slot 9a lesen mit Fehlerdiagnose
# Gibt Hashtable mit Thumbprint, NotAfter zurueck, oder $null bei Fehler.
# ---------------------------------------------------------------------------
function Get-PivCertificate {
    param([string]$Slot = '9a')

    $certFile = [System.IO.Path]::GetTempFileName() + ".pem"

    $result = Invoke-Ykman -Arguments @('piv', 'certificates', 'export', $Slot, $certFile) `
                           -ErrorContext "Zertifikat aus Slot $Slot lesen"

    if ($null -eq $result) {
        # Temp-Datei aufräumen falls ykman sie teilweise erstellt hat
        Remove-Item $certFile -Force -ErrorAction SilentlyContinue

        Write-Host ""
        Write-Host "  Moegliche Ursachen fuer Slot $Slot:" -ForegroundColor Yellow
        Write-Host "  - Slot $Slot ist leer (kein Zertifikat vorhanden)" -ForegroundColor White
        Write-Host "    -> Loesung: Option 1 'YubiKey initialisieren' ausfuehren" -ForegroundColor DarkGray
        Write-Host "  - PIV-Treiber (Minidriver) fehlt oder ist nicht korrekt installiert" -ForegroundColor White
        Write-Host "    -> Loesung: YubiKey Minidriver installieren, dann Neustart" -ForegroundColor DarkGray
        Write-Host "     https://www.yubico.com/support/download/smart-card-drivers-tools/" -ForegroundColor Cyan
        Write-Host "  - YubiKey wurde nach der Treiberinstallation nicht neu eingesteckt" -ForegroundColor White
        return $null
    }

    # Zertifikat laden
    try {
        if (-not (Test-Path $certFile) -or (Get-Item $certFile).Length -eq 0) {
            Write-Host ""
            Write-Host "  [FEHLER] ykman hat keine Zertifikatsdatei erzeugt." -ForegroundColor Red
            Write-Host "  Slot $Slot ist moeglicherweise leer." -ForegroundColor Yellow
            Remove-Item $certFile -Force -ErrorAction SilentlyContinue
            return $null
        }

        $cert       = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($certFile)
        $thumbprint = $cert.Thumbprint
        $notAfter   = $cert.NotAfter
        Remove-Item $certFile -Force -ErrorAction SilentlyContinue

        if ([string]::IsNullOrWhiteSpace($thumbprint)) {
            Write-Host ""
            Write-Host "  [FEHLER] Zertifikat geladen, aber Thumbprint ist leer." -ForegroundColor Red
            Write-Host "  Das Zertifikat in Slot $Slot koennte beschaedigt sein." -ForegroundColor Yellow
            return $null
        }

        return @{ Thumbprint = $thumbprint; NotAfter = $notAfter }
    } catch {
        Remove-Item $certFile -Force -ErrorAction SilentlyContinue
        Write-Host ""
        Write-Host "  [FEHLER] Zertifikatsdatei konnte nicht gelesen werden: $_" -ForegroundColor Red
        Write-Host "  Moeglicherweise ist das Zertifikat in Slot $Slot beschaedigt." -ForegroundColor Yellow
        return $null
    }
}

# ---------------------------------------------------------------------------
# Registry Helper
# ---------------------------------------------------------------------------
function Write-ThumbprintToRegistry {
    param([string]$Thumbprint)
    $regBase = 'HKLM:\SOFTWARE\Jens Kaesler'
    $regKey  = "$regBase\WireGuard Credential Provider"
    if (-not (Test-Path $regBase)) { New-Item $regBase -Force | Out-Null }
    if (-not (Test-Path $regKey))  { New-Item $regKey  -Force | Out-Null }
    # Thumbprint als plain REG_SZ - DPAPI LocalMachine nicht nutzbar
    # da CP als SYSTEM laeuft und SYSTEM keine User-Session DPAPI Blobs
    # entschluesseln kann. Der Thumbprint ist kein Geheimnis.
    Set-ItemProperty -Path $regKey -Name 'SmartcardCertThumbprint' -Value $Thumbprint -Type String
}

# ---------------------------------------------------------------------------
# Benutzerdaten abfragen (AD oder manuell)
# ---------------------------------------------------------------------------
function Get-UserData {
    Write-Host ""
    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host "  Benutzerdaten" -ForegroundColor White
    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray

    do {
        $username = Read-Host "`n  Benutzername (z.B. jkaesler)"
    } while ([string]::IsNullOrWhiteSpace($username))

    Write-Host ""
    Write-Host "  Benutzerinfo-Quelle:" -ForegroundColor White
    Write-Host "  [1] Active Directory abfragen" -ForegroundColor White
    Write-Host "  [2] Manuell eingeben" -ForegroundColor White
    $src = Read-Host "  Auswahl [1/2]"

    $dn = $null
    if ($src -eq '1') {
        Write-Step "Suche AD-Objekt fuer '$username'..."
        try {
            if (Get-Module -ListAvailable -Name ActiveDirectory) {
                Import-Module ActiveDirectory -ErrorAction Stop
                $adUser = Get-ADUser -Identity $username -Properties DistinguishedName -ErrorAction Stop
                $dn = $adUser.DistinguishedName
                Write-Ok "AD-Objekt gefunden: $dn"
            } else {
                throw "ActiveDirectory-Modul nicht verfuegbar"
            }
        } catch {
            Write-Warn "AD-Abfrage fehlgeschlagen: $_"
            Write-Host "  Bitte Distinguished Name manuell eingeben." -ForegroundColor DarkGray
            Write-Host "  Beispiel: CN=Max Mustermann,OU=Users,DC=firma,DC=local" -ForegroundColor DarkGray
            do { $dn = Read-Host "  Distinguished Name" } while ([string]::IsNullOrWhiteSpace($dn))
        }
    } else {
        Write-Host "  Beispiel: CN=Max Mustermann,OU=Users,DC=firma,DC=local" -ForegroundColor DarkGray
        do { $dn = Read-Host "  Distinguished Name" } while ([string]::IsNullOrWhiteSpace($dn))
        Write-Ok "Distinguished Name gesetzt: $dn"
    }

    $cnMatch = $dn | Select-String 'CN=([^,]+)'
    $certCN  = if ($cnMatch) { $cnMatch.Matches.Groups[1].Value.Trim() } else { $username }

    return @{ Username = $username; DN = $dn; CertCN = $certCN }
}

# ---------------------------------------------------------------------------
# Bericht speichern
# ---------------------------------------------------------------------------
function Save-Report {
    param(
        [hashtable]$User,
        [string]$Serial,
        [string]$Model,
        [string]$Thumbprint,
        [datetime]$NotAfter,
        [string]$Pin,
        [string]$Puk,
        [string]$MgmtKey,
        [int]$PinRequired
    )

    $reportDate = Get-Date -Format 'dd.MM.yyyy HH:mm:ss'
    $report = @"
============================================================
  WireGuard Credential Provider - YubiKey PIV Setup-Bericht
  Erstellt: $reportDate
============================================================

GERAET
  Modell:           $Model
  Seriennummer:     $Serial

BENUTZER
  Benutzername:     $($User.Username)
  Distinguished Name: $($User.DN)
  Zertifikat CN:    $($User.CertCN)

ZERTIFIKAT
  Slot:             9A (PIV Authentication)
  Algorithmus:      RSA2048
  Thumbprint:       $Thumbprint
  Gueltig bis:      $($NotAfter.ToString('dd.MM.yyyy'))

ZUGANGSDATEN (VERTRAULICH)
  PIN:              $Pin
  PUK:              $Puk
  Management Key:   $MgmtKey

REGISTRY
  Schluessel:       HKLM\SOFTWARE\Jens Kaesler\WireGuard Credential Provider
  SmartcardEnabled:             1
  SmartcardCertThumbprint:      $Thumbprint
  SmartcardPinRequired:         $PinRequired
  SmartcardPinMinLength:        8
  SmartcardConnectOnInsert:     0
  SmartcardDisconnectOnRemove:  1

============================================================
  HINWEIS: Diese Datei enthaelt vertrauliche Zugangsdaten.
  Bitte sicher aufbewahren - nicht per E-Mail versenden!
============================================================
"@

    Write-Host ""
    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host "  Speicherort fuer den Setup-Bericht" -ForegroundColor White
    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    $defaultPath = "$env:USERPROFILE\Desktop\YubiKey_Setup_$($User.Username)_$(Get-Date -Format 'yyyyMMdd_HHmm').txt"
    Write-Host "  Standard: $defaultPath" -ForegroundColor DarkGray
    $savePath = Read-Host "  Pfad eingeben [Enter fuer Standard]"
    if ([string]::IsNullOrWhiteSpace($savePath)) { $savePath = $defaultPath }

    $saveDir = Split-Path $savePath
    if (-not (Test-Path $saveDir)) { New-Item $saveDir -ItemType Directory -Force | Out-Null }
    [System.IO.File]::WriteAllText($savePath, $report, [System.Text.Encoding]::UTF8)
    Write-Ok "Bericht gespeichert: $savePath"
    return $savePath
}

# ===========================================================================
# MENUE
# ===========================================================================
function Show-Menu {
    Write-Header
    Write-Host ""
    Write-Host "  Was moechten Sie tun?" -ForegroundColor White
    Write-Host ""
    Write-Host "  [1] YubiKey initialisieren" -ForegroundColor White
    Write-Host "      PIV reset, PIN setzen, Zertifikat erstellen, Registry schreiben" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "  [2] Bestehenden YubiKey fuer diesen PC registrieren" -ForegroundColor White
    Write-Host "      Thumbprint vom gesteckten YubiKey lesen und in Registry schreiben" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "  [3] Setup-Bericht exportieren" -ForegroundColor White
    Write-Host "      Thumbprint vom YubiKey lesen, Bericht neu erstellen" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "  [Q] Beenden" -ForegroundColor DarkGray
    Write-Host ""
    return (Read-Host "  Auswahl").Trim()
}

# ===========================================================================
# OPTION 1: YubiKey initialisieren
# ===========================================================================
function Invoke-Initialize {
    Write-Header
    Write-Host ""
    Write-Host "  YubiKey initialisieren" -ForegroundColor Cyan
    Write-Host ""

    # Voraussetzungen
    Write-Step "Pruefe Voraussetzungen..."
    if (-not (Test-Prerequisites)) {
        Write-Host ""
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }
    Write-Ok "ykman gefunden: $(Get-YkmanVersion)"
    Write-Ok "Laeuft als Administrator"

    # YubiKey erkennen
    Write-Step "Suche YubiKey..."
    $ykInfo = Get-YubiKeyInfo
    if ($null -eq $ykInfo) {
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }
    $serial = $ykInfo.Serial
    $model  = $ykInfo.Model
    Write-Ok "Modell: $model | Seriennummer: $serial"

    # PIV-Unterstuetzung prüfen
    if (-not (Test-PivSupport -YkInfo $ykInfo)) {
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }
    Write-Ok "PIV-Unterstuetzung bestaetigt"

    # Benutzerdaten
    $user = Get-UserData

    # Credentials generieren
    Write-Step "Generiere zufaellige Zugangsdaten..."
    $pin = -join (1..8 | ForEach-Object { Get-Random -Minimum 0 -Maximum 10 })
    $puk = -join (1..8 | ForEach-Object { Get-Random -Minimum 0 -Maximum 10 })
    $mgmtKeyBytes = New-Object byte[] 24
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($mgmtKeyBytes)
    $mgmtKey = ($mgmtKeyBytes | ForEach-Object { $_.ToString('X2') }) -join ''
    Write-Ok "PIN, PUK und Management Key generiert"

    # PIN-Abfrage konfigurieren
    Write-Host ""
    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host "  Konfiguration" -ForegroundColor White
    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    $pinPrompt   = Read-Host "`n  PIN-Abfrage vor dem Verbinden aktivieren? [J/n]"
    $pinRequired = if ($pinPrompt -in @('n','N')) { 0 } else { 1 }
    if ($pinRequired -eq 1) { Write-Ok "PIN-Abfrage aktiviert" }
    else                    { Write-Ok "PIN-Abfrage deaktiviert" }

    # Bestaetigung
    Write-Host ""
    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host "  Zusammenfassung" -ForegroundColor White
    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host "  YubiKey:    $model (S/N: $serial)" -ForegroundColor White
    Write-Host "  Benutzer:   $($user.Username)" -ForegroundColor White
    Write-Host "  DN:         $($user.DN)" -ForegroundColor White
    Write-Host "  Zertifikat: CN=$($user.CertCN)" -ForegroundColor White
    Write-Host ""
    Write-Host "  ACHTUNG: PIV-Anwendung wird vollstaendig zurueckgesetzt!" -ForegroundColor Yellow
    Write-Host ""
    $confirm = Read-Host "  Fortfahren? [j/N]"
    if ($confirm -notin @('j','J','y','Y')) { Write-Host "`nAbgebrochen." -ForegroundColor Yellow; return }

    # PIV Reset
    Write-Step "Setze PIV-Anwendung zurueck..."
    $r = Invoke-Ykman -Arguments @('piv','reset','--force') -ErrorContext "PIV-Reset"
    if ($null -eq $r) {
        Write-Warn "PIV-Reset fehlgeschlagen. YubiKey abstecken, neu einstecken und erneut versuchen."
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }
    Write-Ok "PIV-Anwendung zurueckgesetzt"

    # PIN / PUK / Management Key
    Write-Step "Setze PIN..."
    $r = Invoke-Ykman -Arguments @('piv','access','change-pin','--pin','123456','--new-pin',$pin) `
                      -ErrorContext "PIN setzen"
    if ($null -eq $r) {
        Write-Warn "PIN konnte nicht gesetzt werden."
        Write-Info "Moegliche Ursache: Standard-PIN (123456) wurde bereits geaendert."
        Write-Info "Bitte PIV-Reset wiederholen (Abbruch und Option 1 neu starten)."
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }
    Write-Ok "PIN gesetzt"

    Write-Step "Setze PUK..."
    $r = Invoke-Ykman -Arguments @('piv','access','change-puk','--puk','12345678','--new-puk',$puk) `
                      -ErrorContext "PUK setzen"
    if ($null -eq $r) { Write-Warn "PUK-Aenderung fehlgeschlagen (nicht kritisch, PUK bleibt Standard)." }
    else               { Write-Ok "PUK gesetzt" }

    Write-Step "Setze Management Key..."
    $r = Invoke-Ykman -Arguments @('piv','access','change-management-key',
                        '--management-key','010203040506070801020304050607080102030405060708',
                        '--new-management-key',$mgmtKey,'--force') `
                      -ErrorContext "Management Key setzen"
    if ($null -eq $r) { Write-Warn "Management Key nicht geaendert (nicht kritisch)." }
    else               { Write-Ok "Management Key gesetzt" }

    # Schluessel + Zertifikat
    Write-Step "Generiere RSA2048-Schluessel in Slot 9a..."
    $pubkeyFile = [System.IO.Path]::GetTempFileName() + ".pem"
    $r = Invoke-Ykman -Arguments @('piv','keys','generate',
                        '--algorithm','RSA2048','--pin-policy','once','--touch-policy','never',
                        '--management-key',$mgmtKey,'9a',$pubkeyFile) `
                      -ErrorContext "RSA2048-Schluessel generieren"
    if ($null -eq $r) {
        Remove-Item $pubkeyFile -Force -ErrorAction SilentlyContinue
        Write-Host ""
        Write-Host "  Moegliche Ursachen:" -ForegroundColor Yellow
        Write-Host "  - YubiKey 4 unterstuetzt RSA2048 nur in Slot 9a (sollte funktionieren)" -ForegroundColor White
        Write-Host "  - Falscher Management Key (YubiKey war moeglicherweise schon konfiguriert)" -ForegroundColor White
        Write-Host "    -> Loesung: PIV komplett zuruecksetzen (Option 1 neu starten)" -ForegroundColor DarkGray
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }
    Write-Ok "RSA2048-Schluessel generiert"

    Write-Step "Erstelle selbstsigniertes Zertifikat (10 Jahre)..."
    $r = Invoke-Ykman -Arguments @('piv','certificates','generate',
                        '--subject',"CN=$($user.CertCN)",
                        '--valid-days','3650',
                        '--management-key',$mgmtKey,
                        '--pin',$pin,
                        '9a',$pubkeyFile) `
                      -ErrorContext "Selbstsigniertes Zertifikat erstellen"
    Remove-Item $pubkeyFile -Force -ErrorAction SilentlyContinue
    if ($null -eq $r) {
        Write-Host ""
        Write-Host "  Moegliche Ursachen:" -ForegroundColor Yellow
        Write-Host "  - PIN-Eingabe abgelaufen oder PIN gesperrt" -ForegroundColor White
        Write-Host "  - Management Key stimmt nicht ueberein" -ForegroundColor White
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }
    Write-Ok "Zertifikat erstellt"

    # Thumbprint
    Write-Step "Lese Zertifikat-Thumbprint..."
    $certData = Get-PivCertificate -Slot '9a'
    if ($null -eq $certData) {
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }
    $thumbprint = $certData.Thumbprint
    $notAfter   = $certData.NotAfter
    Write-Ok "Thumbprint: $thumbprint"
    Write-Ok "Gueltig bis: $($notAfter.ToString('dd.MM.yyyy'))"

    # Registry
    Write-Step "Schreibe Konfiguration in Registry..."
    $regKey = 'HKLM:\SOFTWARE\Jens Kaesler\WireGuard Credential Provider'
    if (-not (Test-Path 'HKLM:\SOFTWARE\Jens Kaesler')) { New-Item 'HKLM:\SOFTWARE\Jens Kaesler' -Force | Out-Null }
    if (-not (Test-Path $regKey)) { New-Item $regKey -Force | Out-Null }

    Set-ItemProperty -Path $regKey -Name 'SmartcardEnabled'            -Value 1           -Type DWord
    Set-ItemProperty -Path $regKey -Name 'SmartcardCertThumbprint'     -Value $thumbprint -Type String
    Set-ItemProperty -Path $regKey -Name 'SmartcardPinRequired'        -Value $pinRequired -Type DWord
    Set-ItemProperty -Path $regKey -Name 'SmartcardPinMinLength'       -Value 8           -Type DWord
    Set-ItemProperty -Path $regKey -Name 'SmartcardConnectOnInsert'    -Value 0           -Type DWord
    Set-ItemProperty -Path $regKey -Name 'SmartcardDisconnectOnRemove' -Value 1           -Type DWord
    Write-Ok "Registry aktualisiert"

    # Bericht
    $savePath = Save-Report -User $user -Serial $serial -Model $model `
        -Thumbprint $thumbprint -NotAfter $notAfter `
        -Pin $pin -Puk $puk -MgmtKey $mgmtKey -PinRequired $pinRequired

    # Abschluss
    Write-Host ""
    Write-Host "============================================================" -ForegroundColor DarkCyan
    Write-Host "  EINRICHTUNG ABGESCHLOSSEN" -ForegroundColor Green
    Write-Host "============================================================" -ForegroundColor DarkCyan
    Write-Host ""
    Write-Host "  YubiKey:    $model ($serial)" -ForegroundColor White
    Write-Host "  Benutzer:   $($user.Username)" -ForegroundColor White
    Write-Host "  Thumbprint: $thumbprint" -ForegroundColor White
    Write-Host ""
    Write-Host "  PIN: " -NoNewline -ForegroundColor White
    Write-Host $pin -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  Bericht: $savePath" -ForegroundColor DarkGray
    Write-Host "  WireGuard CP Tray neu starten fuer Aenderungen." -ForegroundColor DarkGray
    Write-Host ""
    Read-Host "  [Enter] zurueck zum Menue"
}

# ===========================================================================
# OPTION 2: Thumbprint verschluesseln
# ===========================================================================
function Invoke-EncryptThumbprint {
    Write-Header
    Write-Host ""
    Write-Host "  Bestehenden YubiKey fuer diesen PC registrieren" -ForegroundColor Cyan
    Write-Host ""

    if (-not (Test-Prerequisites)) {
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }

    # YubiKey erkennen
    Write-Step "Suche YubiKey..."
    $ykInfo = Get-YubiKeyInfo
    if ($null -eq $ykInfo) {
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }
    $serial = $ykInfo.Serial
    $model  = $ykInfo.Model
    Write-Ok "YubiKey: $model (S/N: $serial)"

    # PIV-Unterstuetzung prüfen
    if (-not (Test-PivSupport -YkInfo $ykInfo)) {
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }

    # Zertifikat aus Slot 9a lesen
    Write-Step "Lese Zertifikat aus YubiKey Slot 9a..."
    $certData = Get-PivCertificate -Slot '9a'
    if ($null -eq $certData) {
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }
    $thumbprint = $certData.Thumbprint
    Write-Ok "Thumbprint: $thumbprint"

    # Registry schreiben
    Write-Step "Schreibe Thumbprint in Registry..."
    Write-ThumbprintToRegistry -Thumbprint $thumbprint

    $regKey = "HKLM:\SOFTWARE\Jens Kaesler\WireGuard Credential Provider"
    if (-not (Test-Path "HKLM:\SOFTWARE\Jens Kaesler")) { New-Item "HKLM:\SOFTWARE\Jens Kaesler" -Force | Out-Null }
    if (-not (Test-Path $regKey)) { New-Item $regKey -Force | Out-Null }
    Set-ItemProperty -Path $regKey -Name "SmartcardEnabled" -Value 1 -Type DWord
    Write-Ok "SmartcardEnabled            = 1"
    Write-Ok "SmartcardCertThumbprint     = $thumbprint"

    Write-Host ""
    Write-Host "  Fertig. WireGuard CP Tray neu starten fuer Aenderungen." -ForegroundColor Green
    Write-Host ""
    Read-Host "  [Enter] zurueck zum Menue"
}

# ===========================================================================
# OPTION 3: Setup-Bericht exportieren (YubiKey muss gesteckt sein)
# ===========================================================================
function Invoke-ExportReport {
    Write-Header
    Write-Host ""
    Write-Host "  Setup-Bericht exportieren" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Der Thumbprint wird direkt vom gesteckten YubiKey gelesen." -ForegroundColor DarkGray
    Write-Host "  PIN/PUK/Management Key sind nicht wiederherstellbar und" -ForegroundColor DarkGray
    Write-Host "  werden im Bericht als 'unbekannt' eingetragen." -ForegroundColor DarkGray
    Write-Host ""

    if (-not (Test-Prerequisites)) {
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }

    # YubiKey erkennen
    Write-Step "Lese YubiKey-Informationen..."
    $ykInfo = Get-YubiKeyInfo
    if ($null -eq $ykInfo) {
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }
    $serial = $ykInfo.Serial
    $model  = $ykInfo.Model
    Write-Ok "Modell: $model | Seriennummer: $serial"

    # PIV-Unterstuetzung prüfen
    if (-not (Test-PivSupport -YkInfo $ykInfo)) {
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }

    # Thumbprint vom YubiKey lesen
    Write-Step "Lese Zertifikat aus Slot 9a..."
    $certData = Get-PivCertificate -Slot '9a'
    if ($null -eq $certData) {
        Read-Host "  [Enter] zurueck zum Menue"
        return
    }
    $thumbprint = $certData.Thumbprint
    $notAfter   = $certData.NotAfter
    Write-Ok "Thumbprint: $thumbprint"
    Write-Ok "Gueltig bis: $($notAfter.ToString('dd.MM.yyyy'))"

    # Benutzerdaten neu eingeben
    Write-Host ""
    Write-Host "  Da kein alter Bericht vorliegt, bitte Benutzerdaten neu eingeben." -ForegroundColor DarkGray
    $user = Get-UserData

    # PIN-Konfiguration aus Registry lesen (falls vorhanden)
    $pinRequired = 1
    try {
        $regKey = 'HKLM:\SOFTWARE\Jens Kaesler\WireGuard Credential Provider'
        if (Test-Path $regKey) {
            $pinRequired = (Get-ItemProperty $regKey -Name SmartcardPinRequired -ErrorAction SilentlyContinue).SmartcardPinRequired
        }
    } catch {}

    # Bericht erstellen
    $savePath = Save-Report -User $user -Serial $serial -Model $model `
        -Thumbprint $thumbprint -NotAfter $notAfter `
        -Pin "(nicht bekannt - nicht wiederherstellbar)" `
        -Puk "(nicht bekannt - nicht wiederherstellbar)" `
        -MgmtKey "(nicht bekannt - nicht wiederherstellbar)" `
        -PinRequired $pinRequired

    Write-Host ""
    Write-Host "  Bericht exportiert: $savePath" -ForegroundColor Green
    Write-Host ""
    Read-Host "  [Enter] zurueck zum Menue"
}

# ===========================================================================
# HAUPTPROGRAMM
# ===========================================================================
do {
    $choice = Show-Menu
    switch ($choice) {
        '1' { Invoke-Initialize }
        '2' { Invoke-EncryptThumbprint }
        '3' { Invoke-ExportReport }
        { $_ -in @('q','Q') } { Write-Host "`nAuf Wiedersehen." -ForegroundColor DarkGray; exit 0 }
        default { Write-Host "`n  Ungueltige Auswahl. Bitte 1, 2, 3 oder Q eingeben." -ForegroundColor Yellow; Start-Sleep 1 }
    }
} while ($true)
