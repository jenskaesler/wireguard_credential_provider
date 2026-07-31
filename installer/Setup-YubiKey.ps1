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

function Write-Header {
    Clear-Host
    Write-Host "============================================================" -ForegroundColor DarkCyan
    Write-Host "  WireGuard Credential Provider - YubiKey PIV Tool" -ForegroundColor Cyan
    Write-Host "============================================================" -ForegroundColor DarkCyan
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
    if (-not (Get-Command ykman -ErrorAction SilentlyContinue)) {
        Write-Fail "ykman nicht gefunden. Bitte installieren: https://developers.yubico.com/yubikey-manager/"
    }
    Write-Ok "ykman gefunden: $(ykman --version)"

    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) { Write-Fail "Bitte als Administrator ausfuehren." }
    Write-Ok "Laeuft als Administrator"

    $ykInfo = ykman info 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Fail "Kein YubiKey gefunden. Bitte YubiKey einstecken." }
    $serial = ($ykInfo | Select-String 'Serial number:\s*(\d+)').Matches.Groups[1].Value
    $model  = ($ykInfo | Select-String 'Device type:\s*(.+)').Matches.Groups[1].Value.Trim()
    Write-Ok "Modell: $model | Seriennummer: $serial"
    if ($ykInfo -notmatch 'PIV') { Write-Fail "Dieser YubiKey unterstuetzt keine PIV-Anwendung." }
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
    ykman piv reset --force 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Fail "PIV-Reset fehlgeschlagen." }
    Write-Ok "PIV-Anwendung zurueckgesetzt"

    # PIN / PUK / Management Key
    Write-Step "Setze PIN..."
    ykman piv access change-pin --pin 123456 --new-pin $pin 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Fail "PIN-Aenderung fehlgeschlagen." }
    Write-Ok "PIN gesetzt"

    Write-Step "Setze PUK..."
    ykman piv access change-puk --puk 12345678 --new-puk $puk 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Warn "PUK-Aenderung fehlgeschlagen." } else { Write-Ok "PUK gesetzt" }

    Write-Step "Setze Management Key..."
    ykman piv access change-management-key `
        --management-key 010203040506070801020304050607080102030405060708 `
        --new-management-key $mgmtKey `
        --force 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Warn "Management Key nicht geaendert." } else { Write-Ok "Management Key gesetzt" }

    # Schluessel + Zertifikat
    Write-Step "Generiere RSA2048-Schluessel in Slot 9a..."
    $pubkeyFile = [System.IO.Path]::GetTempFileName() + ".pem"
    ykman piv keys generate --algorithm RSA2048 --pin-policy once --touch-policy never `
        --management-key $mgmtKey 9a $pubkeyFile 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Fail "Schluesselgenerierung fehlgeschlagen." }
    Write-Ok "RSA2048-Schluessel generiert"

    Write-Step "Erstelle selbstsigniertes Zertifikat (10 Jahre)..."
    ykman piv certificates generate `
        --subject "CN=$($user.CertCN)" `
        --valid-days 3650 `
        --management-key $mgmtKey `
        --pin $pin `
        9a $pubkeyFile 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Fail "Zertifikatserstellung fehlgeschlagen." }
    Write-Ok "Zertifikat erstellt"
    Remove-Item $pubkeyFile -Force -ErrorAction SilentlyContinue

    # Thumbprint
    Write-Step "Lese Zertifikat-Thumbprint..."
    $certFile = [System.IO.Path]::GetTempFileName() + ".pem"
    ykman piv certificates export 9a $certFile 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Fail "Zertifikatsexport fehlgeschlagen." }
    $cert       = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($certFile)
    $thumbprint = $cert.Thumbprint
    $notAfter   = $cert.NotAfter
    Remove-Item $certFile -Force -ErrorAction SilentlyContinue
    Write-Ok "Thumbprint: $thumbprint"
    Write-Ok "Gueltig bis: $($notAfter.ToString('dd.MM.yyyy'))"

    # Registry
    Write-Step "Schreibe Konfiguration in Registry..."
    $regKey = 'HKLM:\SOFTWARE\Jens Kaesler\WireGuard Credential Provider'
    if (-not (Test-Path 'HKLM:\SOFTWARE\Jens Kaesler')) { New-Item 'HKLM:\SOFTWARE\Jens Kaesler' -Force | Out-Null }
    if (-not (Test-Path $regKey)) { New-Item $regKey -Force | Out-Null }

    Set-ItemProperty -Path $regKey -Name 'SmartcardEnabled'            -Value 1          -Type DWord
    Set-ItemProperty -Path $regKey -Name 'SmartcardCertThumbprint'     -Value $thumbprint -Type String
    Set-ItemProperty -Path $regKey -Name 'SmartcardPinRequired'        -Value $pinRequired -Type DWord
    Set-ItemProperty -Path $regKey -Name 'SmartcardPinMinLength'       -Value 8          -Type DWord
    Set-ItemProperty -Path $regKey -Name 'SmartcardConnectOnInsert'    -Value 0          -Type DWord
    Set-ItemProperty -Path $regKey -Name 'SmartcardDisconnectOnRemove' -Value 1          -Type DWord
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

    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) { Write-Fail "Bitte als Administrator ausfuehren." }

    # Thumbprint direkt vom gesteckten YubiKey lesen
    if (-not (Get-Command ykman -ErrorAction SilentlyContinue)) {
        Write-Fail "ykman nicht gefunden. Bitte installieren: https://developers.yubico.com/yubikey-manager/"
    }

    Write-Step "Lese Zertifikat aus YubiKey Slot 9a..."
    $ykInfo = ykman info 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Fail "Kein YubiKey gefunden. Bitte YubiKey einstecken." }
    $serial = ($ykInfo | Select-String 'Serial number:\s*(\d+)').Matches.Groups[1].Value
    $model  = ($ykInfo | Select-String 'Device type:\s*(.+)').Matches.Groups[1].Value.Trim()
    Write-Ok "YubiKey: $model (S/N: $serial)"

    $certFile = [System.IO.Path]::GetTempFileName() + ".pem"
    ykman piv certificates export 9a $certFile 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "Kein Zertifikat in Slot 9a gefunden. YubiKey initialisieren (Option 1) oder Slot pruefen."
    }
    $cert       = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($certFile)
    $thumbprint = $cert.Thumbprint
    Remove-Item $certFile -Force -ErrorAction SilentlyContinue
    Write-Ok "Thumbprint: $thumbprint"

    Write-Step "Schreibe Thumbprint in Registry..."
    Write-Step "Schreibe in Registry..."
    Write-ThumbprintToRegistry -Thumbprint $thumbprint

    # SmartcardEnabled aktivieren
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

    # ykman pruefen
    if (-not (Get-Command ykman -ErrorAction SilentlyContinue)) {
        Write-Fail "ykman nicht gefunden. Bitte installieren: https://developers.yubico.com/yubikey-manager/"
    }

    # YubiKey pruefen
    Write-Step "Lese YubiKey-Informationen..."
    $ykInfo = ykman info 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Fail "Kein YubiKey gefunden. Bitte YubiKey einstecken." }
    $serial = ($ykInfo | Select-String 'Serial number:\s*(\d+)').Matches.Groups[1].Value
    $model  = ($ykInfo | Select-String 'Device type:\s*(.+)').Matches.Groups[1].Value.Trim()
    Write-Ok "Modell: $model | Seriennummer: $serial"

    # Thumbprint vom YubiKey lesen
    Write-Step "Lese Zertifikat aus Slot 9a..."
    $certFile = [System.IO.Path]::GetTempFileName() + ".pem"
    ykman piv certificates export 9a $certFile 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "Kein Zertifikat in Slot 9a gefunden. YubiKey wurde moeglicherweise noch nicht initialisiert."
    }
    $cert       = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($certFile)
    $thumbprint = $cert.Thumbprint
    $notAfter   = $cert.NotAfter
    Remove-Item $certFile -Force -ErrorAction SilentlyContinue
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

    # Bericht erstellen (ohne PIN/PUK/MgmtKey da nicht wiederherstellbar)
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
