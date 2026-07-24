@echo off
:: ============================================================
:: WireGuard Credential Provider - Installer Build-Script
:: ============================================================

setlocal EnableDelayedExpansion

:: ---- Admin-Check (nur relevant wenn build.bat selbst Installer-Tests ausfuehrt) ----
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] Nicht als Administrator gestartet.
    echo        build.bat selbst benoetigt keine Admin-Rechte.
    echo        Der fertige Installer fragt beim Start nach UAC-Erhoehung.
    echo.
)

echo.
echo ============================================================
echo  WireGuard Credential Provider - Installer Build
echo ============================================================
echo.

:: ---- Pfade ----
set ROOT=%~dp0..
set INSTALLER_DIR=%~dp0
set CONTENT_DIR=%INSTALLER_DIR%content
set NSI_FILE=%INSTALLER_DIR%WireGuardCredentialProvider.nsi
set OUT_FILE=%INSTALLER_DIR%Setup_WireGuardCredentialProvider_x64.exe

set DLL_SRC=%ROOT%\x64\Release\WireGuardCredentialProvider.dll
set SVC_SRC=%ROOT%\x64\Release\WireGuardShutdownService.exe

:: ---- NSIS suchen ----
set MAKENSIS=
where makensis.exe >nul 2>&1
if not errorlevel 1 set MAKENSIS=makensis.exe

if "!MAKENSIS!"=="" (
    if exist "%PROGRAMFILES(X86)%\NSIS\makensis.exe" (
        set "MAKENSIS=%PROGRAMFILES(X86)%\NSIS\makensis.exe"
    )
)
if "!MAKENSIS!"=="" (
    if exist "%PROGRAMFILES%\NSIS\makensis.exe" (
        set "MAKENSIS=%PROGRAMFILES%\NSIS\makensis.exe"
    )
)
if "!MAKENSIS!"=="" (
    echo [FEHLER] makensis.exe nicht gefunden.
    echo         Bitte NSIS installieren: https://nsis.sourceforge.io/
    goto :error
)
echo [OK] NSIS gefunden: !MAKENSIS!

:: ---- Build-Ausgaben prüfen ----
if not exist "%DLL_SRC%" (
    echo [FEHLER] DLL nicht gefunden:
    echo         %DLL_SRC%
    echo.
    echo         Bitte zuerst in Visual Studio bauen:
    echo         Konfiguration: Release ^| x64
    goto :error
)
echo [OK] DLL gefunden: %DLL_SRC%

if not exist "%SVC_SRC%" (
    echo [FEHLER] Shutdown-Service nicht gefunden:
    echo         %SVC_SRC%
    echo.
    echo         Bitte WireGuardShutdownService in Visual Studio bauen:
    echo         Konfiguration: Release ^| x64
    goto :error
)
echo [OK] Service gefunden: %SVC_SRC%

:: ---- Content vorbereiten ----
echo.
echo [1/3] Kopiere Build-Ausgaben nach installer\content\ ...
if not exist "%CONTENT_DIR%" mkdir "%CONTENT_DIR%"
copy /Y "%DLL_SRC%" "%CONTENT_DIR%\WireGuardCredentialProvider.dll" >nul
if errorlevel 1 ( echo [FEHLER] Kopieren der DLL fehlgeschlagen. & goto :error )

copy /Y "%SVC_SRC%" "%CONTENT_DIR%\WireGuardShutdownService.exe" >nul
if errorlevel 1 ( echo [FEHLER] Kopieren des Services fehlgeschlagen. & goto :error )

echo [OK] WireGuardCredentialProvider.dll
echo [OK] WireGuardShutdownService.exe

:: ---- NSIS kompilieren ----
echo.
echo [2/3] Kompiliere NSIS-Skript ...
"!MAKENSIS!" /V2 "%NSI_FILE%"
if errorlevel 1 (
    echo.
    echo [FEHLER] NSIS-Kompilierung fehlgeschlagen.
    echo         Prüfe ob alle Plugins installiert sind:
    echo           - nsProcess:  https://nsis.sourceforge.io/NsProcess_plugin
    echo           - SimpleSC:   https://nsis.sourceforge.io/NSIS_Simple_Service_Plugin
    goto :error
)

:: ---- Ergebnis ----
echo.
echo [3/3] Prüfe Ausgabedatei ...
if not exist "%OUT_FILE%" (
    echo [FEHLER] Installer-EXE wurde nicht erstellt.
    goto :error
)

for %%A in ("%OUT_FILE%") do (
    set SIZE=%%~zA
    set NAME=%%~nxA
)

echo.
echo ============================================================
echo  Installer erfolgreich erstellt:
echo  !NAME!
echo  Groesse: !SIZE! Bytes
echo  Pfad:    %OUT_FILE%
echo.
echo  HINWEIS: Der Installer muss als Administrator ausgefuehrt
echo  werden (UAC-Abfrage erscheint automatisch beim Start).
echo ============================================================
echo.
pause
exit /b 0

:error
echo.
echo ============================================================
echo  Build FEHLGESCHLAGEN - siehe Fehlermeldung oben.
echo ============================================================
echo.
pause
exit /b 1
