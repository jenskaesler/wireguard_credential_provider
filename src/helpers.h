#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef NTDDI_VERSION
#define NTDDI_VERSION   NTDDI_WIN7
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT    0x0601
#endif
#define STRSAFE_NO_DEPRECATE

#include <windows.h>
#include <winuser.h>
#include <strsafe.h>
#include <shlobj.h>
#include <credentialprovider.h>
#include <shlwapi.h>
#include <winsvc.h>
#include "../resources/resource.h"

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "credui.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

#ifndef CPFIS_INTERACTIVE
#define CPFIS_INTERACTIVE  ((CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE)1)
#endif
#ifndef DESKTOP_ALL_ACCESS
#define DESKTOP_ALL_ACCESS 0x01ff
#endif
#ifndef WINSTA_ALL_ACCESS
#define WINSTA_ALL_ACCESS  0x037f
#endif

// ---------------------------------------------------------------------------
// Registry-Schlüssel und Werte
// ---------------------------------------------------------------------------
#define WGCP_REG_KEY          L"SOFTWARE\\WireGuardCredentialProvider"
#define WGCP_REG_EXEPATH      L"ExePath"
#define WGCP_REG_WGEXEPATH    L"WgExePath"
#define WGCP_REG_LABEL        L"TileLabel"
#define WGCP_REG_ICONCONN     L"IconConnected"
#define WGCP_REG_ICONDISCONN  L"IconDisconnected"
#define WGCP_REG_LOGPATH      L"LogPath"
#define WGCP_REG_LOGLEVEL     L"LogLevel"
#define WGCP_REG_LOGRETENTION L"LogRetentionDays"
#define WGCP_REG_INSTALLDIR   L"InstallDir"

// Smartcard / YubiKey PIV
#define WGCP_REG_SC_ENABLED           L"SmartcardEnabled"
#define WGCP_REG_SC_PIN_REQUIRED      L"SmartcardPinRequired"
#define WGCP_REG_SC_PIN_MIN_LENGTH    L"SmartcardPinMinLength"
#define WGCP_REG_SC_PIN_MAX_ATTEMPTS  L"SmartcardPinMaxAttempts"
#define WGCP_REG_SC_READER_NAME       L"SmartcardReaderName"
#define WGCP_REG_SC_CERT_THUMBPRINT   L"SmartcardCertThumbprint"
#define WGCP_REG_SC_TIMEOUT           L"SmartcardTimeout"
#define WGCP_REG_SC_CONNECT_ON_INSERT L"SmartcardConnectOnInsert"
#define WGCP_REG_SC_DISCONNECT_ON_REMOVE L"SmartcardDisconnectOnRemove"

#define WGCP_DEFAULT_EXEPATH      L"C:\\Program Files\\WireGuard\\wireguard.exe"
#define WGCP_DEFAULT_WGEXEPATH    L"C:\\Program Files\\WireGuard\\wg.exe"
#define WGCP_DEFAULT_LABEL        L"WireGuard VPN"
#define WGCP_DEFAULT_ICONCONN     L""
#define WGCP_DEFAULT_ICONDISCONN  L""
#define WGCP_DEFAULT_LOGLEVEL     1
#define WGCP_DEFAULT_LOGRETENTION 7

// Smartcard Defaults
#define WGCP_DEFAULT_SC_ENABLED            0   // deaktiviert
#define WGCP_DEFAULT_SC_PIN_REQUIRED       1   // PIN erforderlich
#define WGCP_DEFAULT_SC_PIN_MIN_LENGTH     4
#define WGCP_DEFAULT_SC_PIN_MAX_ATTEMPTS   3
#define WGCP_DEFAULT_SC_TIMEOUT            10  // Sekunden
#define WGCP_DEFAULT_SC_CONNECT_ON_INSERT  0
#define WGCP_DEFAULT_SC_DISCONNECT_ON_REMOVE 0

#define WG_CONFIG_DIR        L"C:\\Program Files\\WireGuard\\Data\\Configurations\\"
#define WG_CONFIG_EXT        L".conf.dpapi"
#define WG_TUNNEL_SVC_PREFIX L"WireGuardTunnel$"

#define MAX_PATH_WGCP   1024
#define MAX_LABEL_WGCP   256
#define MAX_PROFILES      64

// ---------------------------------------------------------------------------
// Log-Level
// ---------------------------------------------------------------------------
#define WGCP_LOG_OFF   0
#define WGCP_LOG_CRIT  1
#define WGCP_LOG_WARN  2
#define WGCP_LOG_DEBUG 3

// ---------------------------------------------------------------------------
// FIELD_STATE_PAIR
// ---------------------------------------------------------------------------
struct FIELD_STATE_PAIR
{
    CREDENTIAL_PROVIDER_FIELD_STATE             cpfs;
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE cpfis;
};

// ---------------------------------------------------------------------------
// Registry helpers
// ---------------------------------------------------------------------------
inline void ReadRegString(HKEY hKey, PCWSTR pwszValue,
                          WCHAR* pwszOut, DWORD cchOut, PCWSTR pwszDefault)
{
    DWORD dwType = REG_SZ, cbData = cchOut * sizeof(WCHAR);
    if (RegQueryValueExW(hKey, pwszValue, nullptr, &dwType,
                         reinterpret_cast<LPBYTE>(pwszOut), &cbData) != ERROR_SUCCESS
        || dwType != REG_SZ)
        StringCchCopyW(pwszOut, cchOut, (pwszDefault && pwszDefault[0]) ? pwszDefault : L"");
}

inline DWORD ReadRegDword(HKEY hKey, PCWSTR pwszValue, DWORD dwDefault)
{
    DWORD dwType = REG_DWORD, dwVal = 0, cbData = sizeof(dwVal);
    if (RegQueryValueExW(hKey, pwszValue, nullptr, &dwType,
                         reinterpret_cast<LPBYTE>(&dwVal), &cbData) == ERROR_SUCCESS
        && dwType == REG_DWORD)
        return dwVal;
    return dwDefault;
}

// ---------------------------------------------------------------------------
// Log-Pfad auflösen
// Platzhalter: %INSTALLDIR% -> Installationsverzeichnis aus Registry
//              Dateiname kann ddMMyyyy enthalten -> wird ersetzt
// Beispiel: %INSTALLDIR%\logs\wgcp_ddMMyyyy.log
// ---------------------------------------------------------------------------
inline void WGCPResolvLogPath(WCHAR* pwszOut, DWORD cchOut)
{
    // Installationsverzeichnis aus Registry lesen
    WCHAR wszInstDir[MAX_PATH_WGCP] = {};
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, WGCP_REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        ReadRegString(hKey, WGCP_REG_LOGPATH,    pwszOut,    cchOut,         L"");
        ReadRegString(hKey, WGCP_REG_INSTALLDIR, wszInstDir, MAX_PATH_WGCP,  L"");
        RegCloseKey(hKey);
    }

    // Kein Pfad in Registry -> Standard-Fallback mit Datum im Appdata-Verzeichnis
    if (pwszOut[0] == L'\0')
    {
        SYSTEMTIME st = {}; GetLocalTime(&st);
        // Falls InstallDir bekannt, logs\-Unterordner verwenden
        if (wszInstDir[0] != L'\0')
        {
            StringCchPrintfW(pwszOut, cchOut,
                             L"%s\\logs\\wgcp_%02d%02d%04d.log",
                             wszInstDir, st.wDay, st.wMonth, st.wYear);
        }
        else
        {
            StringCchPrintfW(pwszOut, cchOut,
                             L"C:\\ProgramData\\WireGuardCredentialProvider\\logs\\wgcp_%02d%02d%04d.log",
                             st.wDay, st.wMonth, st.wYear);
        }
        return;
    }

    // Datums-Platzhalter ersetzen (ddMMyyyy im Dateinamen)
    SYSTEMTIME st = {}; GetLocalTime(&st);
    WCHAR wszDate[16] = {};
    StringCchPrintfW(wszDate, ARRAYSIZE(wszDate), L"%02d%02d%04d", st.wDay, st.wMonth, st.wYear);

    // Einfaches String-Replace für "ddMMyyyy"
    WCHAR wszResult[MAX_PATH_WGCP] = {};
    PCWSTR pSrc = pwszOut;
    WCHAR* pDst = wszResult;
    DWORD remaining = cchOut - 1;

    while (*pSrc && remaining > 0)
    {
        if (wcsncmp(pSrc, L"ddMMyyyy", 8) == 0)
        {
            StringCchCatW(wszResult, cchOut, wszDate);
            pSrc += 8;
            pDst = wszResult + wcslen(wszResult);
            remaining = cchOut - 1 - static_cast<DWORD>(wcslen(wszResult));
        }
        else
        {
            *pDst++ = *pSrc++;
            remaining--;
        }
    }
    *pDst = L'\0';
    StringCchCopyW(pwszOut, cchOut, wszResult);
}

// ---------------------------------------------------------------------------
// Log-Rotation: Löscht Log-Dateien die älter als dwDays Tage sind
// Sucht im gleichen Verzeichnis wie der aktuelle LogPath nach wgcp_*.log
// ---------------------------------------------------------------------------
inline void WGCPRotateLogs(PCWSTR pwszLogPath, DWORD dwDays)
{
    if (!pwszLogPath || pwszLogPath[0] == L'\0' || dwDays == 0) return;

    // Verzeichnis aus Pfad extrahieren
    WCHAR wszDir[MAX_PATH_WGCP] = {};
    StringCchCopyW(wszDir, MAX_PATH_WGCP, pwszLogPath);
    WCHAR* pLastSlash = wcsrchr(wszDir, L'\\');
    if (!pLastSlash) return;
    *pLastSlash = L'\0';

    // Suchmuster
    WCHAR wszSearch[MAX_PATH_WGCP] = {};
    StringCchPrintfW(wszSearch, MAX_PATH_WGCP, L"%s\\wgcp_*.log", wszDir);

    // Schwellwert: aktuelles Datum minus dwDays Tage als FILETIME
    SYSTEMTIME stNow = {}; GetSystemTime(&stNow);
    FILETIME ftNow = {};   SystemTimeToFileTime(&stNow, &ftNow);
    ULARGE_INTEGER uNow;
    uNow.LowPart  = ftNow.dwLowDateTime;
    uNow.HighPart = ftNow.dwHighDateTime;

    // dwDays in 100-Nanosekunden-Intervalle umrechnen
    ULONGLONG ullThreshold = static_cast<ULONGLONG>(dwDays) * 24ULL * 3600ULL * 10000000ULL;

    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(wszSearch, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        // Datei-Schreibzeit prüfen
        ULARGE_INTEGER uFile;
        uFile.LowPart  = fd.ftLastWriteTime.dwLowDateTime;
        uFile.HighPart = fd.ftLastWriteTime.dwHighDateTime;

        if (uNow.QuadPart > uFile.QuadPart &&
            (uNow.QuadPart - uFile.QuadPart) > ullThreshold)
        {
            WCHAR wszDel[MAX_PATH_WGCP] = {};
            StringCchPrintfW(wszDel, MAX_PATH_WGCP, L"%s\\%s", wszDir, fd.cFileName);
            DeleteFileW(wszDel);
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
}

// ---------------------------------------------------------------------------
// Logger
// Liest Konfiguration aus Registry, löst Datums-Platzhalter auf,
// erstellt Log-Verzeichnis bei Bedarf, schreibt UTF-16 LE mit BOM.
// ---------------------------------------------------------------------------
inline void WGCPLog(DWORD dwLevel, PCWSTR pwszMsg)
{
    // Konfiguration lesen
    DWORD dwCfgLevel    = WGCP_DEFAULT_LOGLEVEL;
    DWORD dwRetention   = WGCP_DEFAULT_LOGRETENTION;
    HKEY  hKey          = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, WGCP_REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        dwCfgLevel  = ReadRegDword(hKey, WGCP_REG_LOGLEVEL,     WGCP_DEFAULT_LOGLEVEL);
        dwRetention = ReadRegDword(hKey, WGCP_REG_LOGRETENTION, WGCP_DEFAULT_LOGRETENTION);
        RegCloseKey(hKey);
    }

    if (dwCfgLevel == WGCP_LOG_OFF || dwLevel > dwCfgLevel) return;

    // Log-Pfad auflösen (mit Datums-Platzhalter)
    WCHAR wszPath[MAX_PATH_WGCP] = {};
    WGCPResolvLogPath(wszPath, MAX_PATH_WGCP);

    // Log-Verzeichnis anlegen
    WCHAR wszDir[MAX_PATH_WGCP] = {};
    StringCchCopyW(wszDir, MAX_PATH_WGCP, wszPath);
    WCHAR* pSlash = wcsrchr(wszDir, L'\\');
    if (pSlash) { *pSlash = L'\0'; SHCreateDirectoryExW(nullptr, wszDir, nullptr); }

    // Datei öffnen/anlegen
    HANDLE hFile = CreateFileW(wszPath, FILE_APPEND_DATA, FILE_SHARE_READ,
                               nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    // BOM beim ersten Schreiben
    LARGE_INTEGER liSize = {}; GetFileSizeEx(hFile, &liSize);
    if (liSize.QuadPart == 0)
    {
        WORD wBOM = 0xFEFF; DWORD dw = 0;
        WriteFile(hFile, &wBOM, sizeof(wBOM), &dw, nullptr);
    }

    // Zeile formatieren
    PCWSTR pwszLvl = (dwLevel==WGCP_LOG_CRIT)?L"[CRIT] "
                   : (dwLevel==WGCP_LOG_WARN)?L"[WARN] ":L"[DEBUG]";
    SYSTEMTIME st = {}; GetLocalTime(&st);
    WCHAR wszLine[2048] = {};
    StringCchPrintfW(wszLine, ARRAYSIZE(wszLine),
                     L"[%04d-%02d-%02d %02d:%02d:%02d] %s %s\r\n",
                     st.wYear, st.wMonth, st.wDay,
                     st.wHour, st.wMinute, st.wSecond,
                     pwszLvl, pwszMsg);
    DWORD dw = 0;
    WriteFile(hFile, wszLine,
              static_cast<DWORD>(wcslen(wszLine) * sizeof(WCHAR)), &dw, nullptr);
    CloseHandle(hFile);

    // Log-Rotation (nur bei CRIT um Performance nicht zu belasten)
    if (dwLevel == WGCP_LOG_CRIT || dwLevel == WGCP_LOG_WARN)
        WGCPRotateLogs(wszPath, dwRetention);
}

#define LOG_CRIT(msg)  WGCPLog(WGCP_LOG_CRIT,  (msg))
#define LOG_WARN(msg)  WGCPLog(WGCP_LOG_WARN,  (msg))
#define LOG_DEBUG(msg) WGCPLog(WGCP_LOG_DEBUG, (msg))

// ---------------------------------------------------------------------------
// String-Duplikation für COM (Aufrufer gibt mit CoTaskMemFree frei)
// ---------------------------------------------------------------------------
inline HRESULT WGCPStrDup(PCWSTR psz, WCHAR** ppwsz)
{
    *ppwsz = nullptr;
    if (!psz || psz[0] == L'\0')
    {
        *ppwsz = static_cast<WCHAR*>(CoTaskMemAlloc(sizeof(WCHAR)));
        if (!*ppwsz) return E_OUTOFMEMORY;
        (*ppwsz)[0] = L'\0';
        return S_OK;
    }
    size_t cch = 0;
    HRESULT hr = StringCchLengthW(psz, STRSAFE_MAX_CCH, &cch);
    if (FAILED(hr)) return hr;
    *ppwsz = static_cast<WCHAR*>(CoTaskMemAlloc((cch + 1) * sizeof(WCHAR)));
    if (!*ppwsz) return E_OUTOFMEMORY;
    return StringCchCopyW(*ppwsz, cch + 1, psz);
}

// ---------------------------------------------------------------------------
// WireGuard-Profil-Enumeration
// ---------------------------------------------------------------------------
inline int WGEnumProfiles(WCHAR profiles[][MAX_PATH_WGCP], int maxProfiles)
{
    int count = 0;
    WCHAR wszSearch[MAX_PATH_WGCP] = {};
    StringCchCopyW(wszSearch, MAX_PATH_WGCP, WG_CONFIG_DIR);
    StringCchCatW(wszSearch,  MAX_PATH_WGCP, L"*");
    StringCchCatW(wszSearch,  MAX_PATH_WGCP, WG_CONFIG_EXT);
    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(wszSearch, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;
    do {
        if (count >= maxProfiles) break;
        WCHAR wszName[MAX_PATH_WGCP] = {};
        StringCchCopyW(wszName, MAX_PATH_WGCP, fd.cFileName);
        size_t extLen  = wcslen(WG_CONFIG_EXT);
        size_t nameLen = wcslen(wszName);
        if (nameLen > extLen) wszName[nameLen - extLen] = L'\0';
        StringCchCopyW(profiles[count++], MAX_PATH_WGCP, wszName);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return count;
}

// ---------------------------------------------------------------------------
// Verbindungsstatus prüfen
// ---------------------------------------------------------------------------
inline bool WGIsTunnelConnected(PCWSTR pwszProfile)
{
    WCHAR wszSvc[MAX_PATH_WGCP] = {};
    StringCchCopyW(wszSvc, MAX_PATH_WGCP, WG_TUNNEL_SVC_PREFIX);
    StringCchCatW(wszSvc,  MAX_PATH_WGCP, pwszProfile);
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return false;
    SC_HANDLE hSvc = OpenServiceW(hSCM, wszSvc, SERVICE_QUERY_STATUS);
    if (!hSvc) { CloseServiceHandle(hSCM); return false; }
    SERVICE_STATUS_PROCESS ssp = {}; DWORD dw = 0;
    (void)QueryServiceStatusEx(hSvc, SC_STATUS_PROCESS_INFO,
                               reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &dw);
    CloseServiceHandle(hSvc); CloseServiceHandle(hSCM);
    return ssp.dwCurrentState == SERVICE_RUNNING;
}

// ---------------------------------------------------------------------------
// Tunnel verbinden: wireguard.exe /installtunnelservice "<pfad>"
// ---------------------------------------------------------------------------
inline bool WGConnect(PCWSTR pwszExePath, PCWSTR pwszProfile)
{
    WCHAR wszConfig[MAX_PATH_WGCP] = {};
    StringCchCopyW(wszConfig, MAX_PATH_WGCP, WG_CONFIG_DIR);
    StringCchCatW(wszConfig,  MAX_PATH_WGCP, pwszProfile);
    StringCchCatW(wszConfig,  MAX_PATH_WGCP, WG_CONFIG_EXT);

    WCHAR wszCmd[MAX_PATH_WGCP * 2] = {};
    StringCchPrintfW(wszCmd, ARRAYSIZE(wszCmd),
                     L"\"%s\" /installtunnelservice \"%s\"", pwszExePath, wszConfig);
    WCHAR d[MAX_PATH_WGCP * 2 + 32] = {};
    StringCchPrintfW(d, ARRAYSIZE(d), L"WGConnect: %s", wszCmd); LOG_DEBUG(d);

    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, wszCmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
        WCHAR e[64] = {}; StringCchPrintfW(e, 64, L"WGConnect err=%lu", GetLastError()); LOG_CRIT(e);
        return false;
    }
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return true;
}

// ---------------------------------------------------------------------------
// Tunnel trennen: wireguard.exe /uninstalltunnelservice <name>
// ---------------------------------------------------------------------------
inline bool WGDisconnect(PCWSTR pwszExePath, PCWSTR pwszProfile)
{
    WCHAR wszCmd[MAX_PATH_WGCP * 2] = {};
    StringCchPrintfW(wszCmd, ARRAYSIZE(wszCmd),
                     L"\"%s\" /uninstalltunnelservice %s", pwszExePath, pwszProfile);
    WCHAR d[MAX_PATH_WGCP * 2 + 32] = {};
    StringCchPrintfW(d, ARRAYSIZE(d), L"WGDisconnect: %s", wszCmd); LOG_DEBUG(d);

    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, wszCmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
        WCHAR e[64] = {}; StringCchPrintfW(e, 64, L"WGDisconnect err=%lu", GetLastError()); LOG_CRIT(e);
        return false;
    }
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return true;
}

// ---------------------------------------------------------------------------
// Traffic-Statistiken via wg.exe show <profil> transfer
// Gibt "↑ X MB   ↓ Y MB" zurück oder "" wenn nicht verfügbar
// ---------------------------------------------------------------------------
inline void WGGetTrafficStats(PCWSTR pwszWgExe, PCWSTR pwszProfile,
                               WCHAR* pwszOut, DWORD cchOut)
{
    pwszOut[0] = L'\0';
    WCHAR wszTmp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, wszTmp);
    WCHAR wszTmpFile[MAX_PATH] = {};
    StringCchPrintfW(wszTmpFile, MAX_PATH, L"%swgcp_stats.txt", wszTmp);

    WCHAR wszCmd[MAX_PATH_WGCP * 2] = {};
    StringCchPrintfW(wszCmd, ARRAYSIZE(wszCmd),
                     L"\"%s\" show \"%s\" transfer", pwszWgExe, pwszProfile);

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hOut = CreateFileW(wszTmpFile, GENERIC_WRITE, 0, &sa,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) return;

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = hOut;
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};
    BOOL bOk = CreateProcessW(nullptr, wszCmd, nullptr, nullptr,
                               TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hOut);
    if (!bOk) { DeleteFileW(wszTmpFile); return; }
    WaitForSingleObject(pi.hProcess, 2000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);

    HANDLE hIn = CreateFileW(wszTmpFile, GENERIC_READ, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hIn == INVALID_HANDLE_VALUE) { DeleteFileW(wszTmpFile); return; }
    char buf[256] = {}; DWORD dwRead = 0;
    (void)ReadFile(hIn, buf, sizeof(buf) - 1, &dwRead, nullptr);
    CloseHandle(hIn);
    DeleteFileW(wszTmpFile);
    if (dwRead == 0) return;

    char* tab1 = strchr(buf, '\t'); if (!tab1) return; tab1++;
    char* tab2 = strchr(tab1, '\t'); if (!tab2) return; *tab2 = '\0'; tab2++;
    char* nl = strchr(tab2, '\n'); if (nl) *nl = '\0';
    char* cr = strchr(tab2, '\r'); if (cr) *cr = '\0';

    long long tx = _atoi64(tab1), rx = _atoi64(tab2);
    auto fmtBytes = [](long long b, WCHAR* out, DWORD cch) {
        if      (b >= 1024LL*1024*1024) StringCchPrintfW(out, cch, L"%.1f GB", b/1073741824.0);
        else if (b >= 1024*1024)        StringCchPrintfW(out, cch, L"%.1f MB", b/1048576.0);
        else if (b >= 1024)             StringCchPrintfW(out, cch, L"%.1f KB", b/1024.0);
        else                            StringCchPrintfW(out, cch, L"%lld B",  b);
    };
    WCHAR wszTx[32] = {}, wszRx[32] = {};
    fmtBytes(tx, wszTx, 32); fmtBytes(rx, wszRx, 32);
    StringCchPrintfW(pwszOut, cchOut, L"\u2191 %s   \u2193 %s", wszTx, wszRx);
}

// ---------------------------------------------------------------------------
// Verbindungstimer: "⏱ Verbunden seit HH:MM:SS"
// ---------------------------------------------------------------------------
inline void WGGetConnectedSince(PCWSTR pwszProfile, WCHAR* pwszOut, DWORD cchOut)
{
    pwszOut[0] = L'\0';
    WCHAR wszSvc[MAX_PATH_WGCP] = {};
    StringCchCopyW(wszSvc, MAX_PATH_WGCP, WG_TUNNEL_SVC_PREFIX);
    StringCchCatW(wszSvc,  MAX_PATH_WGCP, pwszProfile);

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return;
    SC_HANDLE hSvc = OpenServiceW(hSCM, wszSvc, SERVICE_QUERY_STATUS);
    if (!hSvc) { CloseServiceHandle(hSCM); return; }
    SERVICE_STATUS_PROCESS ssp = {}; DWORD dw = 0;
    (void)QueryServiceStatusEx(hSvc, SC_STATUS_PROCESS_INFO,
                               reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &dw);
    CloseServiceHandle(hSvc); CloseServiceHandle(hSCM);
    if (ssp.dwCurrentState != SERVICE_RUNNING || ssp.dwProcessId == 0) return;

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ssp.dwProcessId);
    if (!hProc) return;
    FILETIME ftCreate = {}, ftExit = {}, ftKernel = {}, ftUser = {};
    if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser))
    {
        FILETIME ftNow = {}; GetSystemTimeAsFileTime(&ftNow);
        ULARGE_INTEGER uStart, uNow;
        uStart.LowPart = ftCreate.dwLowDateTime; uStart.HighPart = ftCreate.dwHighDateTime;
        uNow.LowPart   = ftNow.dwLowDateTime;    uNow.HighPart   = ftNow.dwHighDateTime;
        ULONGLONG uDiff = (uNow.QuadPart - uStart.QuadPart) / 10000000ULL;
        DWORD h = (DWORD)(uDiff/3600), m = (DWORD)((uDiff%3600)/60), s = (DWORD)(uDiff%60);
        StringCchPrintfW(pwszOut, cchOut, L"\u23F1 Verbunden seit %02d:%02d:%02d", h, m, s);
    }
    CloseHandle(hProc);
}

// ---------------------------------------------------------------------------
// Smartcard-Konfiguration
// ---------------------------------------------------------------------------
struct WGCPSmartcardConfig
{
    bool    bEnabled;
    bool    bPinRequired;
    DWORD   dwPinMinLength;
    DWORD   dwPinMaxAttempts;
    DWORD   dwTimeout;
    bool    bConnectOnInsert;
    bool    bDisconnectOnRemove;
    WCHAR   wszReaderName[256];
    WCHAR   wszCertThumbprint[128];
};

inline void WGCPLoadSmartcardConfig(WGCPSmartcardConfig& cfg)
{
    ZeroMemory(&cfg, sizeof(cfg));
    cfg.bEnabled             = false;
    cfg.bPinRequired         = true;
    cfg.dwPinMinLength       = WGCP_DEFAULT_SC_PIN_MIN_LENGTH;
    cfg.dwPinMaxAttempts     = WGCP_DEFAULT_SC_PIN_MAX_ATTEMPTS;
    cfg.dwTimeout            = WGCP_DEFAULT_SC_TIMEOUT;
    cfg.bConnectOnInsert     = false;
    cfg.bDisconnectOnRemove  = false;

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, WGCP_REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    cfg.bEnabled            = ReadRegDword(hKey, WGCP_REG_SC_ENABLED,           WGCP_DEFAULT_SC_ENABLED)           != 0;
    cfg.bPinRequired        = ReadRegDword(hKey, WGCP_REG_SC_PIN_REQUIRED,      WGCP_DEFAULT_SC_PIN_REQUIRED)      != 0;
    cfg.dwPinMinLength      = ReadRegDword(hKey, WGCP_REG_SC_PIN_MIN_LENGTH,    WGCP_DEFAULT_SC_PIN_MIN_LENGTH);
    cfg.dwPinMaxAttempts    = ReadRegDword(hKey, WGCP_REG_SC_PIN_MAX_ATTEMPTS,  WGCP_DEFAULT_SC_PIN_MAX_ATTEMPTS);
    cfg.dwTimeout           = ReadRegDword(hKey, WGCP_REG_SC_TIMEOUT,           WGCP_DEFAULT_SC_TIMEOUT);
    cfg.bConnectOnInsert    = ReadRegDword(hKey, WGCP_REG_SC_CONNECT_ON_INSERT, WGCP_DEFAULT_SC_CONNECT_ON_INSERT) != 0;
    cfg.bDisconnectOnRemove = ReadRegDword(hKey, WGCP_REG_SC_DISCONNECT_ON_REMOVE, WGCP_DEFAULT_SC_DISCONNECT_ON_REMOVE) != 0;
    ReadRegString(hKey, WGCP_REG_SC_READER_NAME,     cfg.wszReaderName,     256, L"");
    ReadRegString(hKey, WGCP_REG_SC_CERT_THUMBPRINT, cfg.wszCertThumbprint, 128, L"");

    RegCloseKey(hKey);
}

// ---------------------------------------------------------------------------
// Smartcard / WinSCard Hilfsfunktionen
// ---------------------------------------------------------------------------
#include <winscard.h>
#include <wincrypt.h>
#pragma comment(lib, "winscard.lib")
#pragma comment(lib, "crypt32.lib")

// Ergebnis einer Smartcard-Authentifizierung
enum class WGCPScResult
{
    Success,
    NoCard,
    WrongCard,       // Thumbprint passt nicht
    PinWrong,
    PinLocked,
    Timeout,
    Disabled,
    Error
};

// Prüft ob eine Karte im Reader steckt und gibt den Reader-Namen zurück
inline bool WGCPFindSmartcard(const WGCPSmartcardConfig& cfg,
                               WCHAR* pwszReaderOut, DWORD cchReader)
{
    SCARDCONTEXT hCtx = 0;
    if (SCardEstablishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hCtx) != SCARD_S_SUCCESS)
        return false;

    // Konfigurierter Reader oder alle Reader durchsuchen
    if (cfg.wszReaderName[0] != L'\0')
    {
        // Bestimmten Reader prüfen
        SCARD_READERSTATEW rs = {};
        rs.szReader     = cfg.wszReaderName;
        rs.dwCurrentState = SCARD_STATE_UNAWARE;
        LONG lRet = SCardGetStatusChangeW(hCtx, 0, &rs, 1);
        SCardReleaseContext(hCtx);
        if (lRet == SCARD_S_SUCCESS &&
            (rs.dwEventState & SCARD_STATE_PRESENT))
        {
            StringCchCopyW(pwszReaderOut, cchReader, cfg.wszReaderName);
            return true;
        }
        return false;
    }

    // Alle Reader aufzählen
    DWORD dwLen = SCARD_AUTOALLOCATE;
    LPWSTR pwszReaders = nullptr;
    LONG lRet = SCardListReadersW(hCtx, nullptr,
                                   reinterpret_cast<LPWSTR>(&pwszReaders), &dwLen);
    if (lRet != SCARD_S_SUCCESS || !pwszReaders)
    {
        SCardReleaseContext(hCtx);
        return false;
    }

    bool bFound = false;
    for (LPCWSTR p = pwszReaders; *p; p += wcslen(p) + 1)
    {
        SCARD_READERSTATEW rs = {};
        rs.szReader      = p;
        rs.dwCurrentState = SCARD_STATE_UNAWARE;
        if (SCardGetStatusChangeW(hCtx, 0, &rs, 1) == SCARD_S_SUCCESS &&
            (rs.dwEventState & SCARD_STATE_PRESENT))
        {
            StringCchCopyW(pwszReaderOut, cchReader, p);
            bFound = true;
            break;
        }
    }

    SCardFreeMemory(hCtx, pwszReaders);
    SCardReleaseContext(hCtx);
    return bFound;
}

// Wartet bis eine Karte eingesteckt wird (Timeout in Sekunden, 0 = sofort)
inline bool WGCPWaitForCard(const WGCPSmartcardConfig& cfg,
                             WCHAR* pwszReaderOut, DWORD cchReader)
{
    DWORD dwDeadline = GetTickCount() + cfg.dwTimeout * 1000;
    do {
        if (WGCPFindSmartcard(cfg, pwszReaderOut, cchReader))
            return true;
        Sleep(500);
    } while (GetTickCount() < dwDeadline);
    return false;
}

// Prüft ob Karte entfernt wurde
inline bool WGCPIsCardRemoved(PCWSTR pwszReader)
{
    SCARDCONTEXT hCtx = 0;
    if (SCardEstablishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hCtx) != SCARD_S_SUCCESS)
        return true;

    SCARD_READERSTATEW rs = {};
    rs.szReader      = pwszReader;
    rs.dwCurrentState = SCARD_STATE_UNAWARE;
    LONG lRet = SCardGetStatusChangeW(hCtx, 0, &rs, 1);
    SCardReleaseContext(hCtx);

    if (lRet != SCARD_S_SUCCESS) return true;
    return (rs.dwEventState & SCARD_STATE_EMPTY) != 0;
}

// Prüft Zertifikats-Thumbprint auf der Karte (leer = kein Check)
inline bool WGCPVerifyCertThumbprint(SCARDHANDLE hCard, PCWSTR pwszExpected)
{
    if (!pwszExpected || pwszExpected[0] == L'\0') return true;

    // ATR lesen und Zertifikat via CryptoAPI prüfen
    // Karte als Smartcard-Store öffnen
    HCERTSTORE hStore = CertOpenStore(
        CERT_STORE_PROV_SYSTEM_W, 0, 0,
        CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_READONLY_FLAG,
        L"MY");
    if (!hStore) return false;

    bool bMatch = false;
    PCCERT_CONTEXT pCert = nullptr;
    while ((pCert = CertEnumCertificatesInStore(hStore, pCert)) != nullptr)
    {
        // SHA1-Thumbprint berechnen
        BYTE  rgThumb[20] = {};
        DWORD cbThumb     = sizeof(rgThumb);
        if (!CertGetCertificateContextProperty(pCert, CERT_SHA1_HASH_PROP_ID,
                                               rgThumb, &cbThumb))
            continue;

        // Als Hex-String
        WCHAR wszThumb[48] = {};
        for (DWORD i = 0; i < cbThumb; i++)
            StringCchPrintfW(wszThumb + i*2, 3, L"%02X", rgThumb[i]);

        if (_wcsicmp(wszThumb, pwszExpected) == 0)
        {
            bMatch = true;
            CertFreeCertificateContext(pCert);
            break;
        }
    }
    CertCloseStore(hStore, 0);
    return bMatch;
}

// Haupt-Authentifizierungsfunktion
// pwszPin: PIN (kann nullptr sein wenn bPinRequired=false)
inline WGCPScResult WGCPAuthenticateSmartcard(const WGCPSmartcardConfig& cfg,
                                               PCWSTR pwszPin)
{
    if (!cfg.bEnabled) return WGCPScResult::Disabled;

    // Reader finden
    WCHAR wszReader[256] = {};
    if (!WGCPWaitForCard(cfg, wszReader, 256))
    {
        LOG_WARN(L"Smartcard: Keine Karte gefunden (Timeout)");
        return WGCPScResult::Timeout;
    }

    WCHAR d[512] = {};
    StringCchPrintfW(d, 512, L"Smartcard: Karte gefunden in Reader '%s'", wszReader);
    LOG_DEBUG(d);

    // Verbindung zur Karte herstellen
    SCARDCONTEXT hCtx   = 0;
    SCARDHANDLE  hCard  = 0;
    DWORD        dwProto = 0;

    if (SCardEstablishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hCtx) != SCARD_S_SUCCESS)
        return WGCPScResult::Error;

    LONG lRet = SCardConnectW(hCtx, wszReader,
                               SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
                               &hCard, &dwProto);
    if (lRet != SCARD_S_SUCCESS)
    {
        SCardReleaseContext(hCtx);
        LOG_WARN(L"Smartcard: SCardConnect fehlgeschlagen");
        return WGCPScResult::Error;
    }

    // Thumbprint prüfen
    if (!WGCPVerifyCertThumbprint(hCard, cfg.wszCertThumbprint))
    {
        SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        SCardReleaseContext(hCtx);
        LOG_WARN(L"Smartcard: Zertifikat-Thumbprint stimmt nicht ueberein");
        return WGCPScResult::WrongCard;
    }

    // PIN-Verifizierung via VERIFY APDU (ISO 7816-4)
    if (cfg.bPinRequired && pwszPin && pwszPin[0] != L'\0')
    {
        // PIN von Unicode nach ASCII konvertieren
        char szPin[32] = {};
        WideCharToMultiByte(CP_ACP, 0, pwszPin, -1, szPin, sizeof(szPin)-1, nullptr, nullptr);
        DWORD dwPinLen = (DWORD)strlen(szPin);

        if (dwPinLen < cfg.dwPinMinLength)
        {
            SCardDisconnect(hCard, SCARD_LEAVE_CARD);
            SCardReleaseContext(hCtx);
            return WGCPScResult::PinWrong;
        }

        // PIV VERIFY APDU: CLA=00, INS=20, P1=00, P2=80 (PIV Card Application PIN)
        // Daten: PIN padded mit 0xFF auf 8 Byte
        BYTE apdu[13] = { 0x00, 0x20, 0x00, 0x80, 0x08,
                          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        memcpy(apdu + 5, szPin, min(dwPinLen, 8u));

        BYTE   resp[2]  = {};
        DWORD  dwRecv   = sizeof(resp);
        SCARD_IO_REQUEST ioReq = { dwProto, sizeof(SCARD_IO_REQUEST) };
        const SCARD_IO_REQUEST* pProto = (dwProto == SCARD_PROTOCOL_T0)
                                       ? SCARD_PCI_T0 : SCARD_PCI_T1;

        lRet = SCardTransmit(hCard, pProto, apdu, sizeof(apdu),
                             nullptr, resp, &dwRecv);

        // PIN im Speicher löschen
        SecureZeroMemory(szPin, sizeof(szPin));
        SecureZeroMemory(apdu + 5, 8);

        if (lRet != SCARD_S_SUCCESS)
        {
            SCardDisconnect(hCard, SCARD_LEAVE_CARD);
            SCardReleaseContext(hCtx);
            LOG_WARN(L"Smartcard: SCardTransmit fehlgeschlagen");
            return WGCPScResult::Error;
        }

        // SW1=90, SW2=00 -> Erfolg
        // SW1=63, SW2=CX -> X Versuche verbleibend
        // SW1=69, SW2=83 -> PIN gesperrt
        if (resp[0] == 0x90 && resp[1] == 0x00)
        {
            LOG_DEBUG(L"Smartcard: PIN-Verifikation erfolgreich");
        }
        else if (resp[0] == 0x69 && resp[1] == 0x83)
        {
            SCardDisconnect(hCard, SCARD_LEAVE_CARD);
            SCardReleaseContext(hCtx);
            LOG_WARN(L"Smartcard: PIN gesperrt");
            return WGCPScResult::PinLocked;
        }
        else
        {
            WCHAR e[64] = {};
            DWORD remaining = resp[1] & 0x0F;
            StringCchPrintfW(e, 64,
                             L"Smartcard: PIN falsch. Verbleibende Versuche: %lu", remaining);
            LOG_WARN(e);
            SCardDisconnect(hCard, SCARD_LEAVE_CARD);
            SCardReleaseContext(hCtx);
            return WGCPScResult::PinWrong;
        }
    }

    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    SCardReleaseContext(hCtx);
    LOG_DEBUG(L"Smartcard: Authentifizierung erfolgreich");
    return WGCPScResult::Success;
}
