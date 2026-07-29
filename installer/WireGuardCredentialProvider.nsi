; ============================================================
; WireGuard Credential Provider - NSIS Installer
; ============================================================
; Requirements:
;   - NSIS 3.x
;   - nsProcess Plugin  (https://nsis.sourceforge.io/NsProcess_plugin)
;   - SimpleSC Plugin   (https://nsis.sourceforge.io/NSIS_Simple_Service_Plugin)
;   - MUI2 (included with NSIS)
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
; Version info - fixed version since DLL does not yet carry RC version
; After a rebuild with the new RC file, this can be used instead:
;   !getdllversion "content\WireGuardCredentialProvider.dll" DLL_VER_
;   !define VERSION      "${DLL_VER_1}.${DLL_VER_2}.${DLL_VER_3}.${DLL_VER_4}"
;   !define VERSION_DISP "${DLL_VER_1}.${DLL_VER_2}.${DLL_VER_3}"
; ============================================================
!define VERSION      "2026.7.6.0"
!define VERSION_DISP "2026.7.6"

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
; MUI interface settings
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
; Pages - Installer
; ============================================================
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE   "content\docs\LICENSE.rtf"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; ============================================================
; Pages - Uninstaller
; ============================================================
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; ============================================================
; Languages (German first = default)
; IMPORTANT: LangStrings must come AFTER the Language includes!
; ============================================================
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "English"

; ============================================================
; Language strings - must appear after MUI_LANGUAGE
; Define all strings for one language together
; ============================================================

; --- German ---
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
; General
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
; Version metadata
; ============================================================
VIProductVersion                  "${VERSION}"
VIAddVersionKey "ProductName"     "${APPNAME}"
VIAddVersionKey "CompanyName"     "${PUBLISHER}"
VIAddVersionKey "LegalCopyright"  "${COPYRIGHT}"
VIAddVersionKey "FileDescription" "Installer for ${APPNAME}"
VIAddVersionKey "FileVersion"     "${VERSION}"
VIAddVersionKey "ProductVersion"  "${VERSION}"
VIAddVersionKey "InternalName"    "${INSTALLERNAME}"
VIAddVersionKey "OriginalFilename" "${INSTALLERNAME}.exe"

; ============================================================
; Installer sections
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

        ; Set System32 path explicitly (prevents SysWOW64 redirection)
        StrCpy $9 "$WINDIR\System32"

        ; Disable 64-bit filesystem redirector
        ; (NSIS is 32-bit; without this, files land in SysWOW64)
        ${DisableX64FSRedirection}

        DetailPrint "$(MSG_SVC_STOP)"
        Call StopService

        ; Kill LogonUI to release DLL lock (Windows restarts it automatically)
        DetailPrint "Preparing installation..."
        ExecWait 'regsvr32.exe /s /u "$9\WireGuardCredentialProvider.dll"'
        ExecWait 'taskkill.exe /F /IM LogonUI.exe'
        Sleep 1500

        ; Files into installation directory
        File "content\WireGuardCredentialProvider.dll"
        File "content\WireGuardShutdownService.exe"
        File "content\configure.reg"
        File "content\img\wireguard.ico"

        ; Copy DLL to System32 via robocopy (64-bit process, no WOW64 redirect)
        DetailPrint "Copying DLL to $9..."
        ExecWait 'robocopy.exe "$INSTDIR" "$9" WireGuardCredentialProvider.dll /IS /IT /NJH /NJS /NFL /NDL'

        ; Verify existence via dir command (works around WOW64 FileExists issue)
        ExecWait 'cmd.exe /c if not exist "$9\WireGuardCredentialProvider.dll" exit 1' $R8
        ${If} $R8 != 0
            ${EnableX64FSRedirection}
            MessageBox MB_OK|MB_ICONSTOP "Could not copy DLL to $9.$\nPlease run as Administrator."
            Abort
        ${EndIf}

        ; Register
        DetailPrint "$(MSG_REGSVR)"
        ExecWait 'regsvr32.exe /s "$9\WireGuardCredentialProvider.dll"' $R9
        ${If} $R9 != 0
            ${EnableX64FSRedirection}
            MessageBox MB_OK|MB_ICONSTOP "Registration failed (exit code $R9).$\nPath: $9\WireGuardCredentialProvider.dll"
        ${Else}
            DetailPrint "Credential Provider registered."
        ${EndIf}

        ; Copy Shutdown Service to System32 via robocopy
        ExecWait 'robocopy.exe "$INSTDIR" "$9" WireGuardShutdownService.exe /NJH /NJS /NFL /NDL'
        ExecWait '"$9\WireGuardShutdownService.exe" /install'
        DetailPrint "$(MSG_SVC_START)"

        ; Re-enable filesystem redirector
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

    Section "$(^Name) - Configuration" SecConfig
        SectionIn 1
        SetRegView 64

        ; Always set/update InstallDir and logs directory
        WriteRegStr HKLM "${REG_WGCP}" "InstallDir" "$INSTDIR"
        CreateDirectory "$INSTDIR\logs"

        ; Write config values only on first install
        ; (existing values are not overwritten)
        ClearErrors
        ReadRegStr $R0 HKLM "${REG_WGCP}" "ExePath"
        ${If} $R0 == ""
            DetailPrint "Writing default configuration..."

            WriteRegStr   HKLM "${REG_WGCP}" "ExePath"          "$PROGRAMFILES64\WireGuard\wireguard.exe"
            WriteRegStr   HKLM "${REG_WGCP}" "WgExePath"        "$PROGRAMFILES64\WireGuard\wg.exe"
            WriteRegStr   HKLM "${REG_WGCP}" "TileLabel"        "WireGuard VPN"
            WriteRegStr   HKLM "${REG_WGCP}" "IconConnected"    ""
            WriteRegStr   HKLM "${REG_WGCP}" "IconDisconnected" ""
            WriteRegStr   HKLM "${REG_WGCP}" "LogPath"          ""
            WriteRegDWORD HKLM "${REG_WGCP}" "LogLevel"         1
            WriteRegDWORD HKLM "${REG_WGCP}" "LogRetentionDays" 7
            ; Smartcard (default: disabled)
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardEnabled"            0
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardPinRequired"        1
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardPinMinLength"       4
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardPinMaxAttempts"     3
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardTimeout"            10
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardConnectOnInsert"    0
            WriteRegDWORD HKLM "${REG_WGCP}" "SmartcardDisconnectOnRemove" 0
            WriteRegStr   HKLM "${REG_WGCP}" "SmartcardReaderName"         ""
            WriteRegStr   HKLM "${REG_WGCP}" "SmartcardCertThumbprint"     ""

            DetailPrint "Default configuration written."
        ${Else}
            DetailPrint "Existing configuration preserved (ExePath: $R0)."
        ${EndIf}

        DetailPrint "Log directory: $INSTDIR\logs"

    SectionEnd

SectionGroupEnd

; ============================================================
; Uninstaller sections
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

    Section /o "un.Settings" SecUninstallCfg
        SetRegView 64
        DeleteRegKey HKLM "${REG_WGCP}"
        DetailPrint "Registry configuration removed."
    SectionEnd

SectionGroupEnd

; ============================================================
; Section descriptions
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
; Installer initialization
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
; Uninstaller initialization
; ============================================================
Function un.onInit
    !insertmacro MUI_UNGETLANGUAGE
    SetRegView 64
FunctionEnd

; ============================================================
; Helper functions
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
        DetailPrint "Updating existing installation (version $R0)..."
        Call StopService
        ExecWait 'regsvr32.exe /s /u "$SYSDIR\WireGuardCredentialProvider.dll"'
        Return
    do_fresh:
        DetailPrint "Removing previous version..."
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

