@echo off
:: ============================================================
:: WireGuard Credential Provider - Installer Build-Script
:: ============================================================

setlocal EnableDelayedExpansion

:: ---- Admin check ----
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Please run build.bat as Administrator.
    echo         Right-click build.bat -^> "Run as administrator"
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  WireGuard Credential Provider - Installer Build
echo ============================================================
echo.

:: ---- Paths ----
set ROOT=%~dp0..
set INSTALLER_DIR=%~dp0
set CONTENT_DIR=%INSTALLER_DIR%content
set NSI_FILE=%INSTALLER_DIR%WireGuardCredentialProvider.nsi
set OUT_FILE=%INSTALLER_DIR%Setup_WireGuardCredentialProvider_x64.exe

set DLL_SRC=%ROOT%\x64\Release\WireGuardCredentialProvider.dll
set SVC_SRC=%ROOT%\x64\Release\WireGuardShutdownService.exe
set TRAY_SRC=%ROOT%\tray-app\x64\Release\WireGuardCPTray.exe
set LICENSE_SRC=%ROOT%\LICENSE
set CHECKSUMS_FILE=%INSTALLER_DIR%checksums.txt
:: ---- Locate NSIS ----
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
    echo [ERROR] makensis.exe not found.
    echo         Please install NSIS: https://nsis.sourceforge.io/
    goto :error
)
echo [OK] NSIS found: !MAKENSIS!

:: ---- Verify build outputs ----
if not exist "%DLL_SRC%" (
    echo [ERROR] DLL not found:
    echo         %DLL_SRC%
    echo.
    echo         Please build in Visual Studio first:
    echo         Configuration: Release ^| x64
    goto :error
)
echo [OK] DLL found: %DLL_SRC%

if not exist "%SVC_SRC%" (
    echo [ERROR] Shutdown service not found:
    echo         %SVC_SRC%
    echo.
    echo         Please build WireGuardShutdownService in Visual Studio:
    echo         Configuration: Release ^| x64
    goto :error
)
echo [OK] Service found: %SVC_SRC%

if not exist "%TRAY_SRC%" (
    echo [ERROR] Tray app not found:
    echo         %TRAY_SRC%
    echo.
    echo         Please build WireGuardTray in Visual Studio:
    echo         Configuration: Release ^| x64
    goto :error
)
echo [OK] Tray app found: %TRAY_SRC%

:: ---- Prepare content ----
echo.
echo [1/4] Copying build outputs and license to installer\content\ ...
if not exist "%CONTENT_DIR%" mkdir "%CONTENT_DIR%"
copy /Y "%DLL_SRC%" "%CONTENT_DIR%\WireGuardCredentialProvider.dll" >nul
if errorlevel 1 ( echo [ERROR] Failed to copy DLL. & goto :error )

copy /Y "%SVC_SRC%" "%CONTENT_DIR%\WireGuardShutdownService.exe" >nul
if errorlevel 1 ( echo [ERROR] Failed to copy service. & goto :error )

copy /Y "%TRAY_SRC%" "%CONTENT_DIR%\WireGuardCPTray.exe" >nul
if errorlevel 1 ( echo [ERROR] Failed to copy tray app. & goto :error )

echo [OK] WireGuardCredentialProvider.dll
echo [OK] WireGuardShutdownService.exe
echo [OK] WireGuardCPTray.exe

:: Pull LICENSE from repo root (not checked in as a file)
if not exist "%CONTENT_DIR%\docs" mkdir "%CONTENT_DIR%\docs"
copy /Y "%LICENSE_SRC%" "%CONTENT_DIR%\docs\LICENSE.txt" >nul
if errorlevel 1 ( echo [ERROR] Failed to copy license. & goto :error )
echo [OK] LICENSE.txt (from repo root)

:: ---- Compile NSIS script ----
echo.
echo [2/4] Compiling NSIS script ...
"!MAKENSIS!" /V2 "%NSI_FILE%"
if errorlevel 1 (
    echo.
    echo [ERROR] NSIS compilation failed.
    echo         Check that all plugins are installed:
    echo           - nsProcess:  https://nsis.sourceforge.io/NsProcess_plugin
    echo           - SimpleSC:   https://nsis.sourceforge.io/NSIS_Simple_Service_Plugin
    goto :error
)

:: ---- Verify output ----
echo.
echo [3/4] Verifying output file ...
if not exist "%OUT_FILE%" (
    echo [ERROR] Installer EXE was not created.
    goto :error
)

for %%A in ("%OUT_FILE%") do (
    set SIZE=%%~zA
    set NAME=%%~nxA
)

:: ---- Generate checksums (sha256sum format for GitHub releases) ----
echo.
echo [4/4] Generating checksums.txt ...
powershell -NoProfile -Command "$h=(Get-FileHash '%OUT_FILE%' -Algorithm SHA256).Hash.ToLower();($h+'  !NAME!')|Out-File -FilePath '%CHECKSUMS_FILE%' -Encoding ascii" >nul 2>&1
if errorlevel 1 (
    echo [WARNING] Could not create checksums.txt.
) else (
    echo [OK] checksums.txt
)

echo.
echo ============================================================
echo  Installer built successfully:
echo  !NAME!
echo  Size:      !SIZE! bytes
echo  Path:      %OUT_FILE%
echo  Checksums: %CHECKSUMS_FILE%
echo.
echo  NOTE: The installer must be run as Administrator.
echo  (UAC prompt appears automatically on launch.)
echo ============================================================
echo.
pause
exit /b 0

:error
echo.
echo ============================================================
echo  Build FAILED - see error message above.
echo ============================================================
echo.
pause
exit /b 1
