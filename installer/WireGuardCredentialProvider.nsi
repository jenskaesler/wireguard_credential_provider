; ============================================================
; WireGuard Credential Provider - NSIS Installer
; ============================================================
; Anforderungen:
;   - NSIS 3.x
;   - nsProcess Plugin  (https://nsis.sourceforge.io/NsProcess_plugin)
;   - SimpleSC Plugin   (https://nsis.sourceforge.io/NSIS_Simple_Service_Plugin)
;   - MUI2 (in NSIS enthalten)
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

; ============================================================
; Versionsinfo - feste Version da DLL noch keine RC-Version traegt
; Nach einem Rebuild mit der neuen RC-Datei kann stattdessen verwendet werden:
;   !getdllversion "content\WireGuardCredentialProvider.dll" DLL_VER_
;   !define VERSION      "${DLL_VER_1}.${DLL_VER_2}.${DLL_VER_3}.${DLL_VER_4}"
;   !define VERSION_DISP "${DLL_VER_1}.${DLL_VER_2}.${DLL_VER_3}"
; ============================================================
!define VERSION      "2026.7.5.0"
!define VERSION_DISP "2026.7.5"

; ============================================================
; Konstanten
; ============================================================
!define APPNAME       "WireGuard Credential Provider"
!define PUBLISHER     "Jens Kaesler"
!define INSTALLERNAME "Setup_WireGuardCredentialProvider_x64"
!define UNINSTALL_KEY "{A7C4E8F2-3D91-4B5A-9E76-2F08C134D5B0}"
!define REG_APP       "Software\${PUBLISHER}\${APPNAME}"
!define REG_UNINSTALL "Software\Microsoft\Windows\CurrentVersion\Uninstall\${UNINSTALL_KEY}"
!define REG_WGCP      "SOFTWARE\WireGuardCredentialProvider"

!define HELPURL   "https://github.com/jenskaesler/wireguard-credential-provider"
!define UPDATEURL "https://github.com/jenskaesler/wireguard-credential-provider/releases"
!define ABOUTURL  "https://github.com/jenskaesler/wireguard-credential-provider"

!define /date YEAR "%Y"
!define COPYRIGHT "Copyright 2026 - ${YEAR} ${PUBLISHER}"

!define WG_REG_KEY    "SOFTWARE\WireGuard"
!define WG_DEFAULT_EXE "$PROGRAMFILES64\WireGuard\wireguard.exe"
!define SVC_NAME       "WireGuardShutdownHelper"

; ============================================================
; MUI Interface-Einstellungen
; ============================================================
!define MUI_ABORTWARNING
!define MUI_ICON    "content\img\wgcp.ico"
!define MUI_UNICON  "content\img\wgcp.ico"
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

!define MUI_UNFINISHPAGE_NOAUTOCLOSE

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
; WICHTIG: LangStrings erst NACH den Language-Includes!
; ============================================================
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "English"

; ============================================================
; Sprachstrings - muessen nach MUI_LANGUAGE stehen
; Alle Strings einer Sprache zusammen definieren
; ============================================================

; --- Deutsch ---
LangString DESC_SecMain         ${LANG_GERMAN} "Installiert den Credential Provider und den Shutdown-Dienst auf Ihrem System."
LangString DESC_SecConfig       ${LANG_GERMAN} "Schreibt die Standard-Konfiguration in die Registry (kann jederzeit angepasst werden)."
LangString DESC_SecUninstall    ${LANG_GERMAN} "Entfernt den Credential Provider und den Shutdown-Dienst."
LangString DESC_SecUninstallCfg ${LANG_GERMAN} "Entfernt alle Registry-Einstellungen. WireGuard-Tunnel-Konfigurationen bleiben erhalten."
LangString ERR_NO_64BIT         ${LANG_GERMAN} "Dieses Installationsprogramm erfordert ein 64-Bit-Betriebssystem."
LangString ERR_NO_WG            ${LANG_GERMAN} "WireGuard fuer Windows wurde nicht gefunden.$\n$\nBitte installieren Sie WireGuard zuerst:$\nhttps://www.wireguard.com/install/$\n$\nDie Installation wird abgebrochen."
LangString MSG_UPGRADE          ${LANG_GERMAN} "${APPNAME} ist bereits installiert.$\n$\nMoechten Sie ein Update auf Version ${VERSION_DISP} durchfuehren?$\n$\nDie bestehende Konfiguration bleibt erhalten."
LangString MSG_SVC_STOP         ${LANG_GERMAN} "Stoppe ${SVC_NAME}-Dienst..."
LangString MSG_SVC_START        ${LANG_GERMAN} "Starte ${SVC_NAME}-Dienst..."
LangString MSG_REGSVR           ${LANG_GERMAN} "Registriere Credential Provider..."
LangString MSG_UNREG            ${LANG_GERMAN} "Deregistriere Credential Provider..."

; --- English ---
LangString DESC_SecMain         ${LANG_ENGLISH} "Installs the Credential Provider and the Shutdown Service on your system."
LangString DESC_SecConfig       ${LANG_ENGLISH} "Writes the default configuration to the registry (can be changed at any time)."
LangString DESC_SecUninstall    ${LANG_ENGLISH} "Removes the Credential Provider and the Shutdown Service."
LangString DESC_SecUninstallCfg ${LANG_ENGLISH} "Removes all registry settings. WireGuard tunnel configurations are not affected."
LangString ERR_NO_64BIT         ${LANG_ENGLISH} "This installer requires a 64-bit operating system."
LangString ERR_NO_WG            ${LANG_ENGLISH} "WireGuard for Windows was not found.$\n$\nPlease install WireGuard first:$\nhttps://www.wireguard.com/install/$\n$\nInstallation will be aborted."
LangString MSG_UPGRADE          ${LANG_ENGLISH} "${APPNAME} is already installed.$\n$\nDo you want to update to version ${VERSION_DISP}?$\n$\nYour existing configuration will be preserved."
LangString MSG_SVC_STOP         ${LANG_ENGLISH} "Stopping ${SVC_NAME} service..."
LangString MSG_SVC_START        ${LANG_ENGLISH} "Starting ${SVC_NAME} service..."
LangString MSG_REGSVR           ${LANG_ENGLISH} "Registering Credential Provider..."
LangString MSG_UNREG            ${LANG_ENGLISH} "Unregistering Credential Provider..."

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

; UAC-Manifest explizit einbetten (erzwingt Admin-Prompt bei Start)
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
SectionGroup /e "${APPNAME}" SecGrpInstall

    Section "-UpgradeCheck" SecUpgrade
        Call CheckUpgrade
    SectionEnd

    Section "$(^Name)" SecMain
        SectionIn RO
        SetRegView 64
        SetOverwrite on
        SetOutPath "$INSTDIR"

        ; System32-Pfad explizit setzen (verhindert SysWOW64-Umleitung)
        StrCpy $9 "$WINDIR\System32"

        ; 64-Bit Filesystem-Redirector deaktivieren
        ; (NSIS ist 32-Bit, ohne dies landet alles in SysWOW64)
        ${DisableX64FSRedirection}

        DetailPrint "$(MSG_SVC_STOP)"
        Call StopService

        ; LogonUI beenden damit DLL-Sperre aufgehoben wird (Windows startet es automatisch neu)
        DetailPrint "Bereite Installation vor..."
        ExecWait 'regsvr32.exe /s /u "$9\WireGuardCredentialProvider.dll"'
        ExecWait 'taskkill.exe /F /IM LogonUI.exe'
        Sleep 1500

        ; Dateien ins Installationsverzeichnis
        File "content\WireGuardCredentialProvider.dll"
        File "content\WireGuardShutdownService.exe"
        File "content\configure.reg"
        File "content\img\wireguard.ico"

        ; DLL nach System32 kopieren via robocopy (64-Bit Prozess, kein WOW64 Redirect)
        DetailPrint "Kopiere DLL nach $9..."
        ExecWait 'robocopy.exe "$INSTDIR" "$9" WireGuardCredentialProvider.dll /IS /IT /NJH /NJS /NFL /NDL'

        ; Existenz via dir-Befehl prüfen (umgeht WOW64 FileExists-Problem)
        ExecWait 'cmd.exe /c if not exist "$9\WireGuardCredentialProvider.dll" exit 1' $R8
        ${If} $R8 != 0
            ${EnableX64FSRedirection}
            MessageBox MB_OK|MB_ICONSTOP "Konnte DLL nicht nach $9 kopieren.$\nBitte als Administrator starten."
            Abort
        ${EndIf}

        ; Registrieren
        DetailPrint "$(MSG_REGSVR)"
        ExecWait 'regsvr32.exe /s "$9\WireGuardCredentialProvider.dll"' $R9
        ${If} $R9 != 0
            ${EnableX64FSRedirection}
            MessageBox MB_OK|MB_ICONSTOP "Registrierung fehlgeschlagen (Exit-Code $R9).$\nPfad: $9\WireGuardCredentialProvider.dll"
        ${Else}
            DetailPrint "Credential Provider registriert."
        ${EndIf}

        ; Shutdown-Service via robocopy nach System32
        ExecWait 'robocopy.exe "$INSTDIR" "$9" WireGuardShutdownService.exe /NJH /NJS /NFL /NDL'
        ExecWait '"$9\WireGuardShutdownService.exe" /install'
        DetailPrint "$(MSG_SVC_START)"

        ; Filesystem-Redirector wieder aktivieren
        ${EnableX64FSRedirection}

        WriteUninstaller "$INSTDIR\Uninstall.exe"

        ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
        IntFmt $0 "0x%08X" $0

        WriteRegStr  HKLM "${REG_UNINSTALL}" "DisplayName"           "${APPNAME} ${VERSION_DISP} (x64)"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "DisplayVersion"        "${VERSION_DISP}"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "Publisher"             "${PUBLISHER}"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "UninstallString"       '"$INSTDIR\Uninstall.exe"'
        WriteRegStr  HKLM "${REG_UNINSTALL}" "QuietUninstallString"  '"$INSTDIR\Uninstall.exe" /S'
        WriteRegStr  HKLM "${REG_UNINSTALL}" "InstallLocation"       "$INSTDIR"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "DisplayIcon"           "$INSTDIR\wireguard.ico"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "HelpLink"              "${HELPURL}"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "URLUpdateInfo"         "${UPDATEURL}"
        WriteRegStr  HKLM "${REG_UNINSTALL}" "URLInfoAbout"          "${ABOUTURL}"
        WriteRegDWORD HKLM "${REG_UNINSTALL}" "EstimatedSize"        "$0"
        WriteRegDWORD HKLM "${REG_UNINSTALL}" "NoModify"             1
        WriteRegDWORD HKLM "${REG_UNINSTALL}" "NoRepair"             1

        WriteRegStr  HKLM "${REG_APP}" "InstallDir"       "$INSTDIR"
        WriteRegStr  HKLM "${REG_APP}" "Version"          "${VERSION_DISP}"
        WriteRegStr  HKLM "${REG_APP}" "InstallerVersion" "${VERSION}"

    SectionEnd

    Section "$(^Name) - Konfiguration / Configuration" SecConfig
        SectionIn 1
        SetRegView 64

        ; InstallDir und LogVerzeichnis immer setzen/aktualisieren
        WriteRegStr HKLM "${REG_WGCP}" "InstallDir" "$INSTDIR"
        CreateDirectory "$INSTDIR\logs"

        ; Konfigurationswerte nur bei Erstinstallation schreiben
        ; (vorhandene Werte werden nicht ueberschrieben)
        ClearErrors
        ReadRegStr $R0 HKLM "${REG_WGCP}" "ExePath"
        ${If} $R0 == ""
            DetailPrint "Schreibe Standard-Konfiguration..."

            WriteRegStr   HKLM "${REG_WGCP}" "ExePath"          "$PROGRAMFILES64\WireGuard\wireguard.exe"
            WriteRegStr   HKLM "${REG_WGCP}" "WgExePath"        "$PROGRAMFILES64\WireGuard\wg.exe"
            WriteRegStr   HKLM "${REG_WGCP}" "TileLabel"        "WireGuard VPN"
            WriteRegStr   HKLM "${REG_WGCP}" "IconConnected"    ""
            WriteRegStr   HKLM "${REG_WGCP}" "IconDisconnected" ""
            WriteRegStr   HKLM "${REG_WGCP}" "LogPath"          ""
            WriteRegDWORD HKLM "${REG_WGCP}" "LogLevel"         1
            WriteRegDWORD HKLM "${REG_WGCP}" "LogRetentionDays" 7

            DetailPrint "Standard-Konfiguration geschrieben."
        ${Else}
            DetailPrint "Vorhandene Konfiguration beibehalten (ExePath: $R0)."
        ${EndIf}

        DetailPrint "Log-Verzeichnis: $INSTDIR\logs"

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
        ExecWait '"$9\WireGuardShutdownService.exe" /uninstall'
        Delete /rebootok "$9\WireGuardShutdownService.exe"

        DetailPrint "$(MSG_UNREG)"
        ExecWait 'regsvr32.exe /s /u "$9\WireGuardCredentialProvider.dll"'
        Delete /rebootok "$9\WireGuardCredentialProvider.dll"

        ${EnableX64FSRedirection}

        RMDir /r "$INSTDIR"

        DeleteRegKey HKLM "${REG_UNINSTALL}"
        DeleteRegKey HKLM "${REG_APP}"

    SectionEnd

    Section /o "un.Einstellungen / Settings" SecUninstallCfg
        SetRegView 64
        DeleteRegKey HKLM "${REG_WGCP}"
        DetailPrint "Registry-Konfiguration entfernt."
    SectionEnd

SectionGroupEnd

; ============================================================
; Sektionsbeschreibungen
; ============================================================
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMain}   "$(DESC_SecMain)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecConfig} "$(DESC_SecConfig)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

!insertmacro MUI_UNFUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecUninstall}    "$(DESC_SecUninstall)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecUninstallCfg} "$(DESC_SecUninstallCfg)"
!insertmacro MUI_UNFUNCTION_DESCRIPTION_END

; ============================================================
; Installer - Initialisierung
; ============================================================
Function .onInit
    !insertmacro MUI_LANGDLL_DISPLAY
    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "$(ERR_NO_64BIT)"
        Abort
    ${EndIf}
    SetRegView 64
    Call CheckWireGuard
FunctionEnd

; ============================================================
; Deinstaller - Initialisierung
; ============================================================
Function un.onInit
    !insertmacro MUI_UNGETLANGUAGE
    SetRegView 64
FunctionEnd

; ============================================================
; Hilfsfunktionen
; ============================================================

Function CheckWireGuard
    ClearErrors
    ReadRegStr $R0 HKLM "${WG_REG_KEY}" ""
    ${IfNot} ${Errors}
        Return
    ${EndIf}
    ${If} ${FileExists} "${WG_DEFAULT_EXE}"
        Return
    ${EndIf}
    MessageBox MB_OK|MB_ICONSTOP "$(ERR_NO_WG)"
    Abort
FunctionEnd

Function CheckUpgrade
    ClearErrors
    ReadRegStr $R0 HKLM "${REG_APP}" "Version"
    ${If} ${Errors}
    ${OrIf} $R0 == ""
        Return
    ${EndIf}

    MessageBox MB_YESNO|MB_ICONQUESTION "$(MSG_UPGRADE)" IDYES do_upgrade IDNO do_fresh
    do_upgrade:
        DetailPrint "Aktualisiere bestehende Installation (Version $R0)..."
        Call StopService
        ExecWait 'regsvr32.exe /s /u "$SYSDIR\WireGuardCredentialProvider.dll"'
        Return
    do_fresh:
        DetailPrint "Entferne Vorversion..."
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

