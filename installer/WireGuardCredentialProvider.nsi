; ============================================================
; WireGuard Credential Provider - NSIS Installer
; ============================================================
; Anforderungen:
;   - NSIS 3.x
;   - nsProcess Plugin  (https://nsis.sourceforge.io/NsProcess_plugin)
;   - SimpleSC Plugin   (https://nsis.sourceforge.io/NSIS_Simple_Service_Plugin)
;   - Inetc Plugin      (https://nsis.sourceforge.io/Inetc_plug-in)
;   - MUI2 (in NSIS enthalten)
;
; Silent-Deployment via Baramundi / MDM:
;   Einfach:        Setup_WireGuardCredentialProvider_x64.exe /S
;   Vollstaendig:   Setup_WireGuardCredentialProvider_x64.exe /S /FULL
;   Benutzerdefiniert mit YubiKey-Auswahl:
;     Setup_WireGuardCredentialProvider_x64.exe /S /YKAUTH /YKMINI /YKMGRCLI
;   Deinstallation: Uninstall.exe /S
; ============================================================

Unicode true

; ============================================================
; Includes
; ============================================================
!include "MUI2.nsh"
!include "nsProcess.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "Sections.nsh"

; ============================================================
; Version – automatically extracted from the DLL
; ============================================================
!getdllversion "content\WireGuardCredentialProvider.dll" DLL_VER_
!define VERSION      "${DLL_VER_1}.${DLL_VER_2}.${DLL_VER_3}.${DLL_VER_4}"
!define VERSION_DISP "${DLL_VER_1}.${DLL_VER_2}.${DLL_VER_3}"

; ============================================================
; Konstanten
; ============================================================
!define APPNAME       "WireGuard Credential Provider"
!define PUBLISHER     "Jens Kaesler"
!define INSTALLERNAME "Setup_WireGuardCredentialProvider_x64"
!define UNINSTALL_KEY "{A7C4E8F2-3D91-4B5A-9E76-2F08C134D5B0}"
!define REG_APP       "Software\${PUBLISHER}\${APPNAME}"
!define REG_UNINSTALL "Software\Microsoft\Windows\CurrentVersion\Uninstall\${UNINSTALL_KEY}"
!define REG_WGCP      "SOFTWARE\Jens Kaesler\WireGuard Credential Provider"

!define HELPURL   "https://github.com/jenskaesler/wireguard_credential_provider"
!define UPDATEURL "https://github.com/jenskaesler/wireguard_credential_provider/releases"
!define ABOUTURL  "https://github.com/jenskaesler/wireguard_credential_provider"

!define /date YEAR "%Y"
!define COPYRIGHT "Copyright 2026 - ${YEAR} ${PUBLISHER}"

!define SVC_NAME  "WireGuardShutdownHelper"

; Download-URLs
!define WIREGUARD_URL      "https://download.wireguard.com/windows-client/wireguard-amd64-1.1.msi"
!define WIREGUARD_EXE      "$PROGRAMFILES64\WireGuard\wireguard.exe"
!define YUBIKEY_AUTH_URL   "https://github.com/Yubico/yubioath-flutter/releases/download/7.4.1/yubico-authenticator-7.4.1-win64.msi"
!define YUBIKEY_MINI_URL   "https://downloads.yubico.com/support/YubiKey-Minidriver-latest-x64.msi"
!define YUBIKEY_MGRCLI_URL "https://developers.yubico.com/yubikey-manager/Releases/yubikey-manager-5.9.2-win64.msi"

; ============================================================
; MUI-Einstellungen
; ============================================================
!define MUI_ABORTWARNING
!define MUI_ICON    "content\img\icon.ico"
!define MUI_UNICON  "content\img\icon.ico"
!define MUI_LICENSEPAGE_CHECKBOX

!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_WELCOMEFINISHPAGE_BITMAP   "content\img\wizard.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "content\img\wizard.bmp"

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP   "content\img\header.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "content\img\header.bmp"
!define MUI_HEADERIMAGE_RIGHT

!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
!define MUI_FINISHPAGE_TEXT_LARGE
!define MUI_FINISHPAGE_LINK          "GitHub Repository"
!define MUI_FINISHPAGE_LINK_LOCATION "${ABOUTURL}"
!define MUI_FINISHPAGE_SHOWREADME        "$INSTDIR\docs\LICENSE.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT   "$(MSG_SHOW_README)"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

!define MUI_UNFINISHPAGE_NOAUTOCLOSE
!define MUI_COMPONENTSPAGE_SMALLDESC

; ============================================================
; Seiten - Installer
; ============================================================
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE   "content\docs\LICENSE.rtf"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; ============================================================
; Seiten - Deinstaller
; ============================================================
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; ============================================================
; Sprachen (Deutsch zuerst = Standard)
; ============================================================
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "English"

; ============================================================
; Sprachstrings - Deutsch
; ============================================================
LangString GRP_WGCP             ${LANG_GERMAN} "WireGuard Credential Provider"
LangString GRP_YUBIKEY          ${LANG_GERMAN} "YubiKey Tools (optional)"
LangString DESC_SecMain         ${LANG_GERMAN} "Installiert den WireGuard Credential Provider, den Tray-Dienst und den Shutdown-Dienst. WireGuard wird automatisch installiert falls nicht vorhanden."
LangString DESC_SecConfig       ${LANG_GERMAN} "Schreibt die Standard-Konfiguration in die Registry. Bestehende Einstellungen bleiben bei Updates erhalten."
LangString DESC_SecYKAuth       ${LANG_GERMAN} "Laedt den YubiKey Authenticator (v7.4.1) herunter und installiert ihn. Benoetigt eine Internetverbindung."
LangString DESC_SecYKMini       ${LANG_GERMAN} "Laedt den YubiKey Minidriver (neueste Version) herunter und installiert ihn. Benoetigt eine Internetverbindung."
LangString DESC_SecYKMgrCLI     ${LANG_GERMAN} "Laedt den YubiKey Manager CLI - ykman (v5.9.2) herunter und installiert ihn. Benoetigt eine Internetverbindung."
LangString DESC_SecUninstall    ${LANG_GERMAN} "Entfernt den Credential Provider, den Tray-Dienst und den Shutdown-Dienst vom System."
LangString DESC_SecUninstallCfg ${LANG_GERMAN} "Entfernt alle Registry-Einstellungen. WireGuard-Tunnel-Konfigurationen bleiben erhalten."
LangString ERR_NO_64BIT         ${LANG_GERMAN} "Dieses Installationsprogramm erfordert ein 64-Bit-Betriebssystem."
LangString MSG_UPGRADE          ${LANG_GERMAN} "${APPNAME} ist bereits in Version $R0 installiert.$\n$\nMoechten Sie ein Update auf Version ${VERSION_DISP} durchfuehren?$\n$\nDie bestehende Konfiguration bleibt erhalten."
LangString MSG_SVC_STOP         ${LANG_GERMAN} "Stoppe ${SVC_NAME}-Dienst..."
LangString MSG_SVC_START        ${LANG_GERMAN} "Starte ${SVC_NAME}-Dienst..."
LangString MSG_REGSVR           ${LANG_GERMAN} "Registriere Credential Provider in der Registry..."
LangString MSG_UNREG            ${LANG_GERMAN} "Deregistriere Credential Provider..."
LangString MSG_SHOW_README      ${LANG_GERMAN} "README-Datei oeffnen"
LangString MSG_SETTINGS         ${LANG_GERMAN} "Einstellungen"
LangString MSG_UNINSTALL_SETTINGS ${LANG_GERMAN} "Einstellungen entfernen"
LangString MSG_WG_CHECK         ${LANG_GERMAN} "Pruefe WireGuard-Installation..."
LangString MSG_WG_FOUND         ${LANG_GERMAN} "WireGuard ist bereits installiert."
LangString MSG_WG_DL            ${LANG_GERMAN} "WireGuard nicht gefunden - lade Installer herunter..."
LangString MSG_WG_INST          ${LANG_GERMAN} "Installiere WireGuard (silent)..."
LangString MSG_WG_OK            ${LANG_GERMAN} "WireGuard erfolgreich installiert."
LangString MSG_WG_ERR           ${LANG_GERMAN} "WireGuard konnte nicht installiert werden.$\nDie Installation wird abgebrochen.$\n$\nBitte WireGuard manuell installieren:$\nhttps://www.wireguard.com/install/"
LangString MSG_YKAUTH_DL        ${LANG_GERMAN} "Lade YubiKey Authenticator herunter..."
LangString MSG_YKAUTH_INST      ${LANG_GERMAN} "Installiere YubiKey Authenticator..."
LangString MSG_YKAUTH_OK        ${LANG_GERMAN} "YubiKey Authenticator erfolgreich installiert."
LangString MSG_YKMINI_DL        ${LANG_GERMAN} "Lade YubiKey Minidriver herunter..."
LangString MSG_YKMINI_INST      ${LANG_GERMAN} "Installiere YubiKey Minidriver..."
LangString MSG_YKMINI_OK        ${LANG_GERMAN} "YubiKey Minidriver erfolgreich installiert."
LangString MSG_YKMGRCLI_DL      ${LANG_GERMAN} "Lade YubiKey Manager CLI herunter..."
LangString MSG_YKMGRCLI_INST    ${LANG_GERMAN} "Installiere YubiKey Manager CLI..."
LangString MSG_YKMGRCLI_OK      ${LANG_GERMAN} "YubiKey Manager CLI erfolgreich installiert."
LangString MSG_YK_ERR           ${LANG_GERMAN} "Download fehlgeschlagen ($R0).$\nBitte manuell installieren:"

; ============================================================
; Sprachstrings - Englisch
; ============================================================
LangString GRP_WGCP             ${LANG_ENGLISH} "WireGuard Credential Provider"
LangString GRP_YUBIKEY          ${LANG_ENGLISH} "YubiKey Tools (optional)"
LangString DESC_SecMain         ${LANG_ENGLISH} "Installs the WireGuard Credential Provider, tray service and shutdown service. WireGuard will be installed automatically if not present."
LangString DESC_SecConfig       ${LANG_ENGLISH} "Writes the default configuration to the registry. Existing settings are preserved during updates."
LangString DESC_SecYKAuth       ${LANG_ENGLISH} "Downloads and installs YubiKey Authenticator (v7.4.1). Requires an internet connection."
LangString DESC_SecYKMini       ${LANG_ENGLISH} "Downloads and installs the YubiKey Minidriver (latest version). Requires an internet connection."
LangString DESC_SecYKMgrCLI     ${LANG_ENGLISH} "Downloads and installs the YubiKey Manager CLI - ykman (v5.9.2). Requires an internet connection."
LangString DESC_SecUninstall    ${LANG_ENGLISH} "Removes the Credential Provider, tray service and shutdown service from the system."
LangString DESC_SecUninstallCfg ${LANG_ENGLISH} "Removes all registry settings. WireGuard tunnel configurations are not affected."
LangString ERR_NO_64BIT         ${LANG_ENGLISH} "This installer requires a 64-bit operating system."
LangString MSG_UPGRADE          ${LANG_ENGLISH} "${APPNAME} version $R0 is already installed.$\n$\nDo you want to update to version ${VERSION_DISP}?$\n$\nYour existing configuration will be preserved."
LangString MSG_SVC_STOP         ${LANG_ENGLISH} "Stopping ${SVC_NAME} service..."
LangString MSG_SVC_START        ${LANG_ENGLISH} "Starting ${SVC_NAME} service..."
LangString MSG_REGSVR           ${LANG_ENGLISH} "Registering Credential Provider in registry..."
LangString MSG_UNREG            ${LANG_ENGLISH} "Unregistering Credential Provider..."
LangString MSG_SHOW_README      ${LANG_ENGLISH} "Open README"
LangString MSG_SETTINGS         ${LANG_ENGLISH} "Settings"
LangString MSG_UNINSTALL_SETTINGS ${LANG_ENGLISH} "Remove settings"
LangString MSG_WG_CHECK         ${LANG_ENGLISH} "Checking WireGuard installation..."
LangString MSG_WG_FOUND         ${LANG_ENGLISH} "WireGuard is already installed."
LangString MSG_WG_DL            ${LANG_ENGLISH} "WireGuard not found - downloading installer..."
LangString MSG_WG_INST          ${LANG_ENGLISH} "Installing WireGuard (silent)..."
LangString MSG_WG_OK            ${LANG_ENGLISH} "WireGuard installed successfully."
LangString MSG_WG_ERR           ${LANG_ENGLISH} "WireGuard could not be installed.$\nInstallation will be aborted.$\n$\nPlease install WireGuard manually:$\nhttps://www.wireguard.com/install/"
LangString MSG_YKAUTH_DL        ${LANG_ENGLISH} "Downloading YubiKey Authenticator..."
LangString MSG_YKAUTH_INST      ${LANG_ENGLISH} "Installing YubiKey Authenticator..."
LangString MSG_YKAUTH_OK        ${LANG_ENGLISH} "YubiKey Authenticator installed successfully."
LangString MSG_YKMINI_DL        ${LANG_ENGLISH} "Downloading YubiKey Minidriver..."
LangString MSG_YKMINI_INST      ${LANG_ENGLISH} "Installing YubiKey Minidriver..."
LangString MSG_YKMINI_OK        ${LANG_ENGLISH} "YubiKey Minidriver installed successfully."
LangString MSG_YKMGRCLI_DL      ${LANG_ENGLISH} "Downloading YubiKey Manager CLI..."
LangString MSG_YKMGRCLI_INST    ${LANG_ENGLISH} "Installing YubiKey Manager CLI..."
LangString MSG_YKMGRCLI_OK      ${LANG_ENGLISH} "YubiKey Manager CLI installed successfully."
LangString MSG_YK_ERR           ${LANG_ENGLISH} "Download failed ($R0).$\nPlease install manually:"

; ============================================================
; Allgemein
; ============================================================
BrandingText "${APPNAME} ${VERSION_DISP} by ${PUBLISHER}"
Name          "${APPNAME}"
OutFile       "${INSTALLERNAME}.exe"
InstallDir    "$PROGRAMFILES64\${PUBLISHER}\${APPNAME}"
InstallDirRegKey HKLM "${REG_APP}" "InstallDir"
RequestExecutionLevel admin
ShowInstDetails   show
ShowUninstDetails show

ManifestDPIAware true
ManifestSupportedOS all

!define MUI_LANGDLL_ALLLANGUAGES
!define MUI_LANGDLL_REGISTRY_ROOT      "HKLM"
!define MUI_LANGDLL_REGISTRY_KEY       "${REG_APP}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "InstallerLanguage"
!insertmacro MUI_RESERVEFILE_LANGDLL

; ============================================================
; Versions-Metadaten
; ============================================================
VIProductVersion                  "${VERSION}"
VIAddVersionKey "ProductName"     "${APPNAME}"
VIAddVersionKey "CompanyName"     "${PUBLISHER}"
VIAddVersionKey "LegalCopyright"  "${COPYRIGHT}"
VIAddVersionKey "FileDescription" "Installer fuer ${APPNAME}"
VIAddVersionKey "FileVersion"     "${VERSION}"
VIAddVersionKey "ProductVersion"  "${VERSION}"
VIAddVersionKey "InternalName"    "${INSTALLERNAME}"
VIAddVersionKey "OriginalFilename" "${INSTALLERNAME}.exe"

; ============================================================
; Installer-Sektionen
; ============================================================

SectionGroup /e "$(GRP_WGCP)" SecGrpInstall

    ; Interne Sektion: Upgrade-Pruefung (kein UI)
    Section "-UpgradeCheck" SecUpgrade
        Call CheckUpgrade
    SectionEnd

    ; Pflicht: Hauptkomponenten
    Section "$(^Name)" SecMain
        SectionIn 1 2 3 RO
        SetRegView 64
        SetOverwrite on
        SetOutPath "$INSTDIR"

        StrCpy $9 "$WINDIR\System32"
        ${DisableX64FSRedirection}

        ; -------------------------------------------------------
        ; WireGuard Abhaengigkeit - Pflicht, silent installieren
        ; -------------------------------------------------------
        DetailPrint "$(MSG_WG_CHECK)"
        ${IfNot} ${FileExists} "${WIREGUARD_EXE}"
            DetailPrint "$(MSG_WG_DL)"
            inetc::get /CAPTION "WireGuard" /CANCELTEXT "Abbrechen" \
                "${WIREGUARD_URL}" "$TEMP\wireguard-installer.msi" /END
            Pop $R0
            ${If} $R0 == "OK"
                DetailPrint "$(MSG_WG_INST)"
                ExecWait 'msiexec.exe /i "$TEMP\wireguard-installer.msi" /qn /norestart'
                Delete "$TEMP\wireguard-installer.msi"
                ; Nochmal pruefen ob Installation erfolgreich
                ${IfNot} ${FileExists} "${WIREGUARD_EXE}"
                    ${EnableX64FSRedirection}
                    MessageBox MB_OK|MB_ICONSTOP "$(MSG_WG_ERR)"
                    Abort
                ${EndIf}
                DetailPrint "$(MSG_WG_OK)"
            ${Else}
                ${EnableX64FSRedirection}
                MessageBox MB_OK|MB_ICONSTOP "$(MSG_WG_ERR)"
                Abort
            ${EndIf}
        ${Else}
            DetailPrint "$(MSG_WG_FOUND)"
        ${EndIf}

        ; -------------------------------------------------------
        ; Vorbereitungen: laufende Prozesse beenden
        ; -------------------------------------------------------
        DetailPrint "$(MSG_SVC_STOP)"
        Call StopService

        DetailPrint "Vorbereitung der Installation..."
        ExecWait 'regsvr32.exe /s /u "$9\WireGuardCredentialProvider.dll"'
        ExecWait 'taskkill.exe /F /IM LogonUI.exe'
        ExecWait 'taskkill.exe /F /IM WireGuardCPTray.exe'
        ExecWait 'taskkill.exe /F /IM WireGuardShutdownService.exe'
        Sleep 1500

        ; -------------------------------------------------------
        ; Dateien installieren
        ; -------------------------------------------------------
        DetailPrint "Kopiere Programmdateien..."
        File "content\WireGuardCredentialProvider.dll"
        File "content\WireGuardShutdownService.exe"
        File "content\WireGuardCPTray.exe"
        ; configure.reg removed - all registry keys are written directly by this installer
        File "content\img\icon.ico"

        SetOutPath "$INSTDIR\docs"
        File /r "content\docs\*.*"
        SetOutPath "$INSTDIR"

        ; -------------------------------------------------------
        ; DLL nach System32 (robocopy umgeht WOW64-Umleitung)
        ; -------------------------------------------------------
        DetailPrint "Kopiere DLL nach $9..."
        ExecWait 'robocopy.exe "$INSTDIR" "$9" WireGuardCredentialProvider.dll /IS /IT /NJH /NJS /NFL /NDL'

        ExecWait 'cmd.exe /c if not exist "$9\WireGuardCredentialProvider.dll" exit 1' $R8
        ${If} $R8 != 0
            ${EnableX64FSRedirection}
            MessageBox MB_OK|MB_ICONSTOP "DLL konnte nicht nach $9 kopiert werden.$\nBitte als Administrator ausfuehren."
            Abort
        ${EndIf}

        ; -------------------------------------------------------
        ; Credential Provider registrieren
        ; -------------------------------------------------------
        DetailPrint "$(MSG_REGSVR)"
        ExecWait 'regsvr32.exe /s "$9\WireGuardCredentialProvider.dll"' $R9
        ${If} $R9 != 0
            ${EnableX64FSRedirection}
            MessageBox MB_OK|MB_ICONSTOP "Registrierung fehlgeschlagen (Exit-Code $R9).$\nPfad: $9\WireGuardCredentialProvider.dll"
        ${Else}
            DetailPrint "Credential Provider erfolgreich registriert."
        ${EndIf}

        ; -------------------------------------------------------
        ; Shutdown-Dienst installieren
        ; -------------------------------------------------------
        DetailPrint "Installiere Shutdown-Dienst..."
        ExecWait 'robocopy.exe "$INSTDIR" "$9" WireGuardShutdownService.exe /NJH /NJS /NFL /NDL'
        ExecWait '"$9\WireGuardShutdownService.exe" /install'
        DetailPrint "$(MSG_SVC_START)"

        ; -------------------------------------------------------
        ; Tray-App Autostart
        ; -------------------------------------------------------
        DetailPrint "Konfiguriere Tray-App Autostart..."
        DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "WireGuard"
        WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WireGuardCPTray" '"$INSTDIR\WireGuardCPTray.exe"'

        ; -------------------------------------------------------
        ; WireGuard Startmenu-Shortcut sichern und entfernen
        ; -------------------------------------------------------
        SetShellVarContext all
        CreateDirectory "$INSTDIR\backup"
        ${If} ${FileExists} "$SMPROGRAMS\WireGuard.lnk"
            CopyFiles /SILENT "$SMPROGRAMS\WireGuard.lnk" "$INSTDIR\backup\WireGuard.lnk"
            Delete "$SMPROGRAMS\WireGuard.lnk"
            DetailPrint "WireGuard Startmenue-Shortcut gesichert."
        ${ElseIf} ${FileExists} "$SMPROGRAMS\WireGuard\WireGuard.lnk"
            CopyFiles /SILENT "$SMPROGRAMS\WireGuard\WireGuard.lnk" "$INSTDIR\backup\WireGuard.lnk"
            Delete "$SMPROGRAMS\WireGuard\WireGuard.lnk"
            Delete "$SMPROGRAMS\WireGuard\*.lnk"
            RMDir "$SMPROGRAMS\WireGuard"
            DetailPrint "WireGuard Startmenue-Shortcut gesichert (Unterordner)."
        ${Else}
            DetailPrint "Kein WireGuard Startmenue-Shortcut gefunden."
        ${EndIf}

        ; -------------------------------------------------------
        ; Start Menu shortcut (runs as Administrator)
        ; The RunAsAdministrator flag (byte 21, bit 5) is set via
        ; PowerShell binary patch on the .lnk file.
        ; -------------------------------------------------------
        SetShellVarContext all
        CreateDirectory "$SMPROGRAMS\${APPNAME}"
        CreateShortcut "$SMPROGRAMS\${APPNAME}\WireGuard CP Tray.lnk" \
            "$INSTDIR\WireGuardCPTray.exe" "" \
            "$INSTDIR\WireGuardCPTray.exe" 0 SW_SHOWNORMAL "" \
            "WireGuard Credential Provider Tray"
        ; Write a temp PS1 script to set the RunAsAdministrator flag on the shortcut
        ; (avoids NSIS $ variable conflicts with PowerShell variables)
        FileOpen $R5 "$TEMP\wgcp_setadmin.ps1" w
        FileWrite $R5 "$$lnk = '$SMPROGRAMS\${APPNAME}\WireGuard CP Tray.lnk'$\r$\n"
        FileWrite $R5 "$$b = [System.IO.File]::ReadAllBytes($$lnk)$\r$\n"
        FileWrite $R5 "$$b[21] = $$b[21] -bor 0x20$\r$\n"
        FileWrite $R5 "[System.IO.File]::WriteAllBytes($$lnk, $$b)$\r$\n"
        FileClose $R5
        nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$TEMP\wgcp_setadmin.ps1"'
        Delete "$TEMP\wgcp_setadmin.ps1"
        DetailPrint "Start Menu shortcut created (runs as Administrator)."

        ${EnableX64FSRedirection}

        ; -------------------------------------------------------
        ; Tray-App starten (kein Neu-Anmelden noetig)
        ; -------------------------------------------------------
        DetailPrint "Starte WireGuard CP Tray-App..."
        Exec '"$INSTDIR\WireGuardCPTray.exe"'

        ; -------------------------------------------------------
        ; Deinstaller und Registry-Eintraege
        ; -------------------------------------------------------
        WriteUninstaller "$INSTDIR\Uninstall.exe"

        ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
        IntFmt $0 "0x%08X" $0

        WriteRegStr  HKLM "${REG_UNINSTALL}" "DisplayName"          "${APPNAME} ${VERSION_DISP} (x64)"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "DisplayVersion"       "${VERSION_DISP}"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "Publisher"            "${PUBLISHER}"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "UninstallString"      '"$INSTDIR\Uninstall.exe"'
        WriteRegStr  HKLM "${REG_UNINSTALL}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
        WriteRegStr  HKLM "${REG_UNINSTALL}" "InstallLocation"      "$INSTDIR"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "DisplayIcon"          "$INSTDIR\icon.ico"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "HelpLink"             "${HELPURL}"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "URLUpdateInfo"        "${UPDATEURL}"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "URLInfoAbout"         "${ABOUTURL}"
        WriteRegDWORD HKLM "${REG_UNINSTALL}" "EstimatedSize"       "$0"
        WriteRegDWORD HKLM "${REG_UNINSTALL}" "NoModify"            1
        WriteRegDWORD HKLM "${REG_UNINSTALL}" "NoRepair"            1

        WriteRegStr  HKLM "${REG_APP}" "InstallDir"       "$INSTDIR"
        WriteRegStr  HKLM "${REG_APP}" "Version"          "${VERSION_DISP}"
        WriteRegStr  HKLM "${REG_APP}" "InstallerVersion" "${VERSION}"

        DetailPrint "Installation abgeschlossen."

    SectionEnd

    ; Pflicht: Registry-Konfiguration
    Section "$(^Name) - $(MSG_SETTINGS)" SecConfig
        SectionIn 1 2 3 RO
        SetRegView 64

        DetailPrint "Schreibe Registry-Konfiguration..."

        WriteRegStr HKLM "${REG_WGCP}" "InstallDir" "$INSTDIR"
        CreateDirectory "$INSTDIR\logs"

        ; Diese Werte werden bei jedem Install/Update gesetzt
        WriteRegStr HKLM "${REG_WGCP}" "ExePath"   "$PROGRAMFILES64\WireGuard\wireguard.exe"
        WriteRegStr HKLM "${REG_WGCP}" "WgExePath" "$PROGRAMFILES64\WireGuard\wg.exe"
        WriteRegStr HKLM "${REG_WGCP}" "ConfigDir" "$PROGRAMFILES64\WireGuard\Data\Configurations\"

        ; Neue Keys seit letztem Release - immer prüfen und ggf. anlegen
        ClearErrors
        ReadRegDWORD $R0 HKLM "${REG_WGCP}" "HandshakeTimeoutSec"
        ${If} $R0 == ""
            WriteRegDWORD HKLM "${REG_WGCP}" "HandshakeTimeoutSec" 0
        ${EndIf}

        ; Weitere Werte nur beim Erstinstall (Benutzereinstellungen erhalten)
        ClearErrors
        ReadRegStr $R0 HKLM "${REG_WGCP}" "TileLabel"
        ${If} $R0 == ""
            DetailPrint "Schreibe Standard-Konfiguration..."
            WriteRegStr   HKLM "${REG_WGCP}" "TileLabel"                   "WireGuard VPN"
            WriteRegStr   HKLM "${REG_WGCP}" "IconConnected"               ""
            WriteRegStr   HKLM "${REG_WGCP}" "IconDisconnected"            ""
            WriteRegStr   HKLM "${REG_WGCP}" "LogPath"                     ""
            WriteRegDWORD HKLM "${REG_WGCP}" "LogLevel"                    1
            WriteRegDWORD HKLM "${REG_WGCP}" "LogRetentionDays"            7
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardEnabled"            0
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardPinRequired"        1
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardPinMinLength"       4
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardPinMaxAttempts"     3
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardTimeout"            10
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardConnectOnInsert"    0
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardDisconnectOnRemove" 0
            WriteRegStr   HKLM "${REG_WGCP}" "SmartcardReaderName"         ""
            WriteRegStr   HKLM "${REG_WGCP}" "SmartcardCertThumbprint"     ""
            WriteRegDWORD HKLM "${REG_WGCP}" "HandshakeTimeoutSec"         0
            DetailPrint "Standard-Konfiguration geschrieben."
        ${Else}
            DetailPrint "Bestehende Konfiguration beibehalten."
        ${EndIf}

    SectionEnd

SectionGroupEnd

; ============================================================
; Optionale YubiKey-Komponenten
; ============================================================
SectionGroup "$(GRP_YUBIKEY)" SecGrpYubiKey

    Section "YubiKey Authenticator" SecYKAuth
        SectionIn 2 3
        SetRegView 64
        DetailPrint "$(MSG_YKAUTH_DL)"
        inetc::get /CAPTION "YubiKey Authenticator" /CANCELTEXT "Abbrechen" \
            "${YUBIKEY_AUTH_URL}" "$TEMP\yubico-authenticator.msi" /END
        Pop $R0
        ${If} $R0 == "OK"
            DetailPrint "$(MSG_YKAUTH_INST)"
            ExecWait 'msiexec.exe /i "$TEMP\yubico-authenticator.msi" /qn /norestart'
            Delete "$TEMP\yubico-authenticator.msi"
            DetailPrint "$(MSG_YKAUTH_OK)"
        ${Else}
            MessageBox MB_OK|MB_ICONEXCLAMATION "$(MSG_YK_ERR)$\n${YUBIKEY_AUTH_URL}"
        ${EndIf}
    SectionEnd

    Section "YubiKey Minidriver" SecYKMini
        SectionIn 2 3
        SetRegView 64
        DetailPrint "$(MSG_YKMINI_DL)"
        inetc::get /CAPTION "YubiKey Minidriver" /CANCELTEXT "Abbrechen" \
            "${YUBIKEY_MINI_URL}" "$TEMP\yubikey-minidriver.msi" /END
        Pop $R0
        ${If} $R0 == "OK"
            DetailPrint "$(MSG_YKMINI_INST)"
            ExecWait 'msiexec.exe /i "$TEMP\yubikey-minidriver.msi" /qn /norestart'
            Delete "$TEMP\yubikey-minidriver.msi"
            DetailPrint "$(MSG_YKMINI_OK)"
        ${Else}
            MessageBox MB_OK|MB_ICONEXCLAMATION "$(MSG_YK_ERR)$\n${YUBIKEY_MINI_URL}"
        ${EndIf}
    SectionEnd

    Section "YubiKey Manager CLI (ykman)" SecYKMgrCLI
        SectionIn 2 3
        SetRegView 64
        DetailPrint "$(MSG_YKMGRCLI_DL)"
        inetc::get /CAPTION "YubiKey Manager CLI" /CANCELTEXT "Abbrechen" \
            "${YUBIKEY_MGRCLI_URL}" "$TEMP\yubikey-manager-cli.msi" /END
        Pop $R0
        ${If} $R0 == "OK"
            DetailPrint "$(MSG_YKMGRCLI_INST)"
            ExecWait 'msiexec.exe /i "$TEMP\yubikey-manager-cli.msi" /qn /norestart'
            Delete "$TEMP\yubikey-manager-cli.msi"
            DetailPrint "$(MSG_YKMGRCLI_OK)"
        ${Else}
            MessageBox MB_OK|MB_ICONEXCLAMATION "$(MSG_YK_ERR)$\n${YUBIKEY_MGRCLI_URL}"
        ${EndIf}
    SectionEnd

SectionGroupEnd

; ============================================================
; Deinstaller-Sektionen
; ============================================================
SectionGroup /e "un.${APPNAME}" SecGrpUninstall

    Section "un.$(^Name)" SecUninstall
        SectionIn RO
        SetRegView 64

        StrCpy $9 "$WINDIR\System32"
        ${DisableX64FSRedirection}

        DetailPrint "$(MSG_SVC_STOP)"
        ExecWait 'taskkill.exe /F /IM WireGuardCPTray.exe'
        DetailPrint "Entferne Tray-App Autostart..."
        DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WireGuardCPTray"

        ; Remove tray app Start Menu shortcut
        SetShellVarContext all
        Delete "$SMPROGRAMS\${APPNAME}\WireGuard CP Tray.lnk"
        RMDir "$SMPROGRAMS\${APPNAME}"

        ; Restore WireGuard Start Menu shortcut
        ${If} ${FileExists} "$INSTDIR\backup\WireGuard.lnk"
            CopyFiles /SILENT "$INSTDIR\backup\WireGuard.lnk" "$SMPROGRAMS\WireGuard.lnk"
            DetailPrint "WireGuard Startmenue-Shortcut wiederhergestellt."
        ${EndIf}

        DetailPrint "Deinstalliere Shutdown-Dienst..."
        ExecWait '"$9\WireGuardShutdownService.exe" /uninstall'
        Delete /rebootok "$9\WireGuardShutdownService.exe"
        Delete /rebootok "$INSTDIR\WireGuardCPTray.exe"

        DetailPrint "$(MSG_UNREG)"
        ExecWait 'regsvr32.exe /s /u "$9\WireGuardCredentialProvider.dll"'
        Delete /rebootok "$9\WireGuardCredentialProvider.dll"

        ${EnableX64FSRedirection}

        DetailPrint "Entferne Programmverzeichnis..."
        RMDir /r "$INSTDIR"

        DetailPrint "Bereinige Registry..."
        DeleteRegKey HKLM "${REG_UNINSTALL}"
        DeleteRegKey HKLM "${REG_APP}"

        DetailPrint "Deinstallation abgeschlossen."

    SectionEnd

    Section "un.$(MSG_UNINSTALL_SETTINGS)" SecUninstallCfg
        SetRegView 64
        DetailPrint "Entferne Registry-Konfiguration..."
        DeleteRegKey HKLM "${REG_WGCP}"
        DetailPrint "Registry-Konfiguration entfernt."
    SectionEnd

SectionGroupEnd

; ============================================================
; Beschreibungen
; ============================================================
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMain}     "$(DESC_SecMain)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecConfig}   "$(DESC_SecConfig)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecYKAuth}   "$(DESC_SecYKAuth)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecYKMini}   "$(DESC_SecYKMini)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecYKMgrCLI} "$(DESC_SecYKMgrCLI)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

!insertmacro MUI_UNFUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecUninstall}    "$(DESC_SecUninstall)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecUninstallCfg} "$(DESC_SecUninstallCfg)"
!insertmacro MUI_UNFUNCTION_DESCRIPTION_END

; ============================================================
; Initialisierung
; ============================================================
Function .onInit
    ; Im Silent-Modus: keine Sprachauswahl
    IfSilent +2
        !insertmacro MUI_LANGDLL_DISPLAY

    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "$(ERR_NO_64BIT)"
        Abort
    ${EndIf}
    SetRegView 64

    ; Standard: Einfach - YubiKey deselektiert
    !insertmacro UnselectSection ${SecYKAuth}
    !insertmacro UnselectSection ${SecYKMini}
    !insertmacro UnselectSection ${SecYKMgrCLI}

    ; Silent-Parameter auswerten fuer Baramundi/MDM-Deployment
    ; /FULL    -> Vollstaendig (alle YubiKey-Tools)
    ; /YKAUTH  -> nur YubiKey Authenticator
    ; /YKMINI  -> nur YubiKey Minidriver
    ; /YKMGRCLI -> nur YubiKey Manager CLI
    ${GetParameters} $R0

    ClearErrors
    ${GetOptions} $R0 "/FULL" $R1
    ${IfNot} ${Errors}
        !insertmacro SelectSection ${SecYKAuth}
        !insertmacro SelectSection ${SecYKMini}
        !insertmacro SelectSection ${SecYKMgrCLI}
    ${EndIf}

    ClearErrors
    ${GetOptions} $R0 "/YKAUTH" $R1
    ${IfNot} ${Errors}
        !insertmacro SelectSection ${SecYKAuth}
    ${EndIf}

    ClearErrors
    ${GetOptions} $R0 "/YKMINI" $R1
    ${IfNot} ${Errors}
        !insertmacro SelectSection ${SecYKMini}
    ${EndIf}

    ClearErrors
    ${GetOptions} $R0 "/YKMGRCLI" $R1
    ${IfNot} ${Errors}
        !insertmacro SelectSection ${SecYKMgrCLI}
    ${EndIf}

FunctionEnd

Function un.onInit
    !insertmacro MUI_UNGETLANGUAGE
    SetRegView 64
FunctionEnd

; ============================================================
; Hilfsfunktionen
; ============================================================
Function CheckUpgrade
    ClearErrors
    ReadRegStr $R0 HKLM "${REG_APP}" "Version"
    ${If} ${Errors}
    ${OrIf} $R0 == ""
        Return  ; Erstinstall - kein Upgrade-Dialog
    ${EndIf}

    ; Im Silent-Modus: automatisch upgraden ohne Dialog
    IfSilent do_upgrade

    MessageBox MB_YESNO|MB_ICONQUESTION "$(MSG_UPGRADE)" IDYES do_upgrade IDNO do_fresh
    do_upgrade:
        DetailPrint "Update von Version $R0 auf ${VERSION_DISP}..."
        Call StopService
        ExecWait 'regsvr32.exe /s /u "$SYSDIR\WireGuardCredentialProvider.dll"'
        Return
    do_fresh:
        DetailPrint "Entferne vorherige Version..."
        ReadRegStr $R1 HKLM "${REG_APP}" "InstallDir"
        ${If} ${FileExists} "$R1\Uninstall.exe"
            ExecWait '"$R1\Uninstall.exe" /S'
            Sleep 2000
        ${EndIf}
        Return
FunctionEnd

Function StopService
    SimpleSC::StopService "${SVC_NAME}" 1 30
    ClearErrors
FunctionEnd
