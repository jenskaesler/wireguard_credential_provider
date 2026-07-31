#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef NTDDI_VERSION
#define NTDDI_VERSION   NTDDI_WIN7
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT    0x0601
#endif
#ifndef STRSAFE_NO_DEPRECATE
#define STRSAFE_NO_DEPRECATE
#endif

#include <windows.h>
#include <winuser.h>
#include <strsafe.h>
#include <winsvc.h>
#include <wincrypt.h>
#include <wincred.h>   // CredUIPromptForCredentialsW  // DATA_BLOB, CryptProtectData, CryptUnprotectData
#include <dpapi.h>     // CRYPTPROTECT_LOCAL_MACHINE

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")

#ifndef WGCP_TRAY_BUILD
  // CP DLL only
  #include <shlobj.h>
  #include <credentialprovider.h>
  #include <shlwapi.h>
  #include "../resources/resource.h"
  #pragma comment(lib, "shlwapi.lib")
  #pragma comment(lib, "credui.lib")
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
#else
  // Tray build needs these for CoTaskMemAlloc and SHCreateDirectoryExW
  #include <objbase.h>       // CoTaskMemAlloc / CoTaskMemFree
  #include <shlobj.h>        // SHCreateDirectoryExW
  #include "../../resources/resource.h"
  #pragma comment(lib, "ole32.lib")
  #pragma comment(lib, "shell32.lib")
#endif

// ---------------------------------------------------------------------------
// Registry key and values
// ---------------------------------------------------------------------------
#define WGCP_REG_KEY          L"SOFTWARE\\Jens Kaesler\\WireGuard Credential Provider"
#define WGCP_REG_EXEPATH      L"ExePath"
#define WGCP_REG_WGEXEPATH    L"WgExePath"
#define WGCP_REG_LABEL        L"TileLabel"
#define WGCP_REG_ICONCONN     L"IconConnected"
#define WGCP_REG_ICONDISCONN  L"IconDisconnected"
#define WGCP_REG_LOGPATH      L"LogPath"
#define WGCP_REG_LOGLEVEL     L"LogLevel"
#define WGCP_REG_LOGRETENTION L"LogRetentionDays"
#define WGCP_REG_INSTALLDIR   L"InstallDir"
#define WGCP_REG_CONFIGDIR    L"ConfigDir"

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
#define WGCP_DEFAULT_LOGLEVEL     3  // DEBUG: log everything by default
#define WGCP_DEFAULT_LOGRETENTION 7

// Smartcard defaults
#define WGCP_DEFAULT_SC_ENABLED            0   // disabled
#define WGCP_DEFAULT_SC_PIN_REQUIRED       1   // PIN required
#define WGCP_DEFAULT_SC_PIN_MIN_LENGTH     4
#define WGCP_DEFAULT_SC_PIN_MAX_ATTEMPTS   3
#define WGCP_DEFAULT_SC_TIMEOUT            10  // seconds
#define WGCP_DEFAULT_SC_CONNECT_ON_INSERT  0
#define WGCP_DEFAULT_SC_DISCONNECT_ON_REMOVE 0

// WG_CONFIG_DIR is now read from registry (ConfigDir).
// Use WGGetConfigDir() instead of the old static define.
#define WG_CONFIG_DIR_DEFAULT L"C:\\Program Files\\WireGuard\\Data\\Configurations\\"
#define WG_CONFIG_EXT        L".conf.dpapi"
#define WG_TUNNEL_SVC_PREFIX L"WireGuardTunnel$"

#define MAX_PATH_WGCP   1024
#define MAX_LABEL_WGCP   256
#define MAX_PROFILES      64

// ---------------------------------------------------------------------------
// Log levels
// ---------------------------------------------------------------------------
#define WGCP_LOG_OFF   0
#define WGCP_LOG_CRIT  1
#define WGCP_LOG_WARN  2
#define WGCP_LOG_DEBUG 3

#ifndef WGCP_TRAY_BUILD
// ---------------------------------------------------------------------------
// FIELD_STATE_PAIR  (CP DLL only – needs credentialprovider.h)
// ---------------------------------------------------------------------------
struct FIELD_STATE_PAIR
{
    CREDENTIAL_PROVIDER_FIELD_STATE             cpfs;
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE cpfis;
};
#endif // WGCP_TRAY_BUILD

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
// WGGetConfigDir – reads ConfigDir from registry, falls back to ProgramData
// ---------------------------------------------------------------------------
inline void WGGetConfigDir(WCHAR* pwszOut, DWORD cchOut)
{
    pwszOut[0] = L'\0';
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, WGCP_REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        ReadRegString(hKey, WGCP_REG_CONFIGDIR, pwszOut, cchOut, L"");

        // If ConfigDir is empty: fall back to InstallDir\configurations
        if (pwszOut[0] == L'\0')
        {
            WCHAR wszInstDir[MAX_PATH_WGCP] = {};
            ReadRegString(hKey, WGCP_REG_INSTALLDIR, wszInstDir, MAX_PATH_WGCP, L"");
            if (wszInstDir[0] != L'\0')
            {
                StringCchCopyW(pwszOut, cchOut, wszInstDir);
                StringCchCatW(pwszOut,  cchOut, L"\\configurations");
            }
        }
        RegCloseKey(hKey);
    }

    // Last resort fallback if InstallDir is also empty
    if (pwszOut[0] == L'\0')
        StringCchCopyW(pwszOut, cchOut, L"C:\\Windows\\Temp\\wgcp_configurations");

    // Ensure trailing backslash
    size_t len = wcslen(pwszOut);
    if (len > 0 && pwszOut[len-1] != L'\\')
        StringCchCatW(pwszOut, cchOut, L"\\");
}

// ---------------------------------------------------------------------------
// Resolve log path
// Placeholder: %INSTALLDIR% -> installation directory from registry
//              Filename may contain ddMMyyyy -> will be substituted
// Example: %INSTALLDIR%\logs\wgcp_ddMMyyyy.log

// Returns true if running in the tray app (post-logon) vs CP DLL (pre-logon/SYSTEM)
static inline bool WGCPIsTraySuffix()
{
    WCHAR wszExe[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, wszExe, MAX_PATH);
    return (wcsstr(wszExe, L"Tray") != nullptr || wcsstr(wszExe, L"tray") != nullptr);
}

// ---------------------------------------------------------------------------
inline void WGCPResolvLogPath(WCHAR* pwszOut, DWORD cchOut)
{
    // Read installation directory from registry
    WCHAR wszInstDir[MAX_PATH_WGCP] = {};
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, WGCP_REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        ReadRegString(hKey, WGCP_REG_LOGPATH,    pwszOut,    cchOut,         L"");
        ReadRegString(hKey, WGCP_REG_INSTALLDIR, wszInstDir, MAX_PATH_WGCP,  L"");
        RegCloseKey(hKey);
    }

    // No path in registry -> default fallback with date in ProgramData
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
                             WGCPIsTraySuffix() ? L"C:\\Windows\\Temp\\wgcp_%02d%02d%04d.log"
                                            : L"C:\\Windows\\Temp\\wgcp_%02d%02d%04d_cp.log",
                             st.wDay, st.wMonth, st.wYear);
        }
        return;
    }

    // Replace date placeholder (ddMMyyyy in filename)
    SYSTEMTIME st = {}; GetLocalTime(&st);
    WCHAR wszDate[16] = {};
    StringCchPrintfW(wszDate, ARRAYSIZE(wszDate), L"%02d%02d%04d", st.wDay, st.wMonth, st.wYear);

    // Simple string replace for "ddMMyyyy"
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
// Log rotation: deletes log files older than dwDays days
// Searches the same directory as the current LogPath for wgcp_*.log
// ---------------------------------------------------------------------------
inline void WGCPRotateLogs(PCWSTR pwszLogPath, DWORD dwDays)
{
    if (!pwszLogPath || pwszLogPath[0] == L'\0' || dwDays == 0) return;

    // Extract directory from path
    WCHAR wszDir[MAX_PATH_WGCP] = {};
    StringCchCopyW(wszDir, MAX_PATH_WGCP, pwszLogPath);
    WCHAR* pLastSlash = wcsrchr(wszDir, L'\\');
    if (!pLastSlash) return;
    *pLastSlash = L'\0';

    // Search pattern
    WCHAR wszSearch[MAX_PATH_WGCP] = {};
    StringCchPrintfW(wszSearch, MAX_PATH_WGCP, L"%s\\wgcp_*.log", wszDir);

    // Threshold: current date minus dwDays as FILETIME
    SYSTEMTIME stNow = {}; GetSystemTime(&stNow);
    FILETIME ftNow = {};   SystemTimeToFileTime(&stNow, &ftNow);
    ULARGE_INTEGER uNow;
    uNow.LowPart  = ftNow.dwLowDateTime;
    uNow.HighPart = ftNow.dwHighDateTime;

    // Convert dwDays to 100-nanosecond intervals
    ULONGLONG ullThreshold = static_cast<ULONGLONG>(dwDays) * 24ULL * 3600ULL * 10000000ULL;

    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(wszSearch, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        // Check file write time
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
// Reads config from registry, resolves date placeholders,
// creates log directory if needed, writes UTF-16 LE with BOM.
// ---------------------------------------------------------------------------
inline void WGCPLog(DWORD dwLevel, PCWSTR pwszMsg)
{
    // Read configuration
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

    // Resolve log path (with date placeholder)
    WCHAR wszPath[MAX_PATH_WGCP] = {};
    WGCPResolvLogPath(wszPath, MAX_PATH_WGCP);

    // Create log directory - if it fails (SYSTEM cannot write to Program Files)
    // switch to C:\Windows\Temp which is always writable
    {
        WCHAR wszDir[MAX_PATH_WGCP] = {};
        StringCchCopyW(wszDir, MAX_PATH_WGCP, wszPath);
        WCHAR* pSlash = wcsrchr(wszDir, L'\\');
        if (pSlash)
        {
            *pSlash = L'\0';
            DWORD dwDirErr = SHCreateDirectoryExW(nullptr, wszDir, nullptr);
            if (dwDirErr != ERROR_SUCCESS && dwDirErr != ERROR_ALREADY_EXISTS)
            {
                SYSTEMTIME stTmp = {}; GetLocalTime(&stTmp);
                StringCchPrintfW(wszPath, MAX_PATH_WGCP,
                    WGCPIsTraySuffix() ? L"C:\\Windows\\Temp\\wgcp_%02d%02d%04d.log"
                                       : L"C:\\Windows\\Temp\\wgcp_%02d%02d%04d_cp.log",
                    stTmp.wDay, stTmp.wMonth, stTmp.wYear);
            }
        }
    }

    // Open or create log file
    HANDLE hFile = CreateFileW(wszPath, FILE_APPEND_DATA,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        // Fallback: C:\Windows\Temp (always writable, even as SYSTEM)
        SYSTEMTIME stFb = {}; GetLocalTime(&stFb);
        StringCchPrintfW(wszPath, MAX_PATH_WGCP,
            WGCPIsTraySuffix() ? L"C:\\Windows\\Temp\\wgcp_%02d%02d%04d.log"
                               : L"C:\\Windows\\Temp\\wgcp_%02d%02d%04d_cp.log",
            stFb.wDay, stFb.wMonth, stFb.wYear);
        hFile = CreateFileW(wszPath, FILE_APPEND_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;
    }

    // Format log line as UTF-8
    PCWSTR pwszLvl = (dwLevel==WGCP_LOG_CRIT)?L"[CRIT] "
                   : (dwLevel==WGCP_LOG_WARN)?L"[WARN] ":L"[DEBUG]";
    SYSTEMTIME st = {}; GetLocalTime(&st);
    WCHAR wszLine[2048] = {};
    StringCchPrintfW(wszLine, ARRAYSIZE(wszLine),
                     L"[%04d-%02d-%02d %02d:%02d:%02d] %s %s\r\n",
                     st.wYear, st.wMonth, st.wDay,
                     st.wHour, st.wMinute, st.wSecond,
                     pwszLvl, pwszMsg);
    // Convert to UTF-8 for writing
    char szLine[4096] = {};
    WideCharToMultiByte(CP_UTF8, 0, wszLine, -1, szLine, sizeof(szLine), nullptr, nullptr);
    DWORD dw = 0;
    WriteFile(hFile, szLine, lstrlenA(szLine), &dw, nullptr);
    CloseHandle(hFile);

    // Log rotation (only on CRIT/WARN to avoid performance impact)
    if (dwLevel == WGCP_LOG_CRIT || dwLevel == WGCP_LOG_WARN)
        WGCPRotateLogs(wszPath, dwRetention);
}

#define LOG_CRIT(msg)  WGCPLog(WGCP_LOG_CRIT,  (msg))
#define LOG_WARN(msg)  WGCPLog(WGCP_LOG_WARN,  (msg))
#define LOG_DEBUG(msg) WGCPLog(WGCP_LOG_DEBUG, (msg))

// ---------------------------------------------------------------------------
// String duplication for COM (caller frees with CoTaskMemFree)
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
// WireGuard profile enumeration
// ---------------------------------------------------------------------------
inline int WGEnumProfiles(WCHAR profiles[][MAX_PATH_WGCP], int maxProfiles)
{
    int count = 0;
    WCHAR wszSearch[MAX_PATH_WGCP] = {};
    WGGetConfigDir(wszSearch, MAX_PATH_WGCP);
    // Search pattern: enumerate all *.dpapi files, then verify the full extension manually.
    // FindFirstFileW does not support "*.conf.dpapi" (compound extension).
    StringCchCatW(wszSearch, MAX_PATH_WGCP, L"*.dpapi");
    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(wszSearch, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;
    do {
        if (count >= maxProfiles) break;
        // Only accept files with exactly ".conf.dpapi" extension
        size_t nameLen = wcslen(fd.cFileName);
        size_t extLen  = wcslen(WG_CONFIG_EXT);
        if (nameLen <= extLen) continue;
        if (_wcsicmp(fd.cFileName + nameLen - extLen, WG_CONFIG_EXT) != 0) continue;
        WCHAR wszName[MAX_PATH_WGCP] = {};
        StringCchCopyW(wszName, MAX_PATH_WGCP, fd.cFileName);
        wszName[nameLen - extLen] = L'\0';  // Strip extension
        StringCchCopyW(profiles[count++], MAX_PATH_WGCP, wszName);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return count;
}

// ---------------------------------------------------------------------------
// Check connection status
// ---------------------------------------------------------------------------
inline bool WGIsTunnelConnected(PCWSTR pwszProfile)
{
    WCHAR wszSvc[MAX_PATH_WGCP] = {};
    StringCchCopyW(wszSvc, MAX_PATH_WGCP, WG_TUNNEL_SVC_PREFIX);
    StringCchCatW(wszSvc,  MAX_PATH_WGCP, pwszProfile);
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM)
    {
        WCHAR e[128] = {};
        StringCchPrintfW(e, 128, L"WGIsTunnelConnected: OpenSCManager failed err=%lu", GetLastError());
        LOG_WARN(e); return false;
    }
    SC_HANDLE hSvc = OpenServiceW(hSCM, wszSvc, SERVICE_QUERY_STATUS);
    if (!hSvc)
    {
        WCHAR e[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(e, ARRAYSIZE(e), L"WGIsTunnelConnected: Service '%s' not found err=%lu", wszSvc, GetLastError());
        LOG_DEBUG(e); CloseServiceHandle(hSCM); return false;
    }
    SERVICE_STATUS_PROCESS ssp = {}; DWORD dw = 0;
    QueryServiceStatusEx(hSvc, SC_STATUS_PROCESS_INFO,
                         reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &dw);
    CloseServiceHandle(hSvc); CloseServiceHandle(hSCM);
    WCHAR d[MAX_PATH_WGCP + 64] = {};
    StringCchPrintfW(d, ARRAYSIZE(d), L"WGIsTunnelConnected: '%s' state=%lu", wszSvc, ssp.dwCurrentState);
    LOG_DEBUG(d);
    return ssp.dwCurrentState == SERVICE_RUNNING;
}

// ---------------------------------------------------------------------------
// Connect tunnel: wireguard.exe /installtunnelservice "<path>"
// ---------------------------------------------------------------------------
inline bool WGConnect(PCWSTR pwszExePath, PCWSTR pwszProfile)
{
    WCHAR wszConfig[MAX_PATH_WGCP] = {};
    WGGetConfigDir(wszConfig, MAX_PATH_WGCP);
    StringCchCatW(wszConfig,  MAX_PATH_WGCP, pwszProfile);
    StringCchCatW(wszConfig,  MAX_PATH_WGCP, WG_CONFIG_EXT);

    // Config-Datei existiert?
    if (GetFileAttributesW(wszConfig) == INVALID_FILE_ATTRIBUTES)
    {
        WCHAR e[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(e, ARRAYSIZE(e), L"WGConnect: Config nicht gefunden: '%s' err=%lu",
                         wszConfig, GetLastError());
        LOG_CRIT(e); return false;
    }

    // wireguard.exe existiert?
    if (GetFileAttributesW(pwszExePath) == INVALID_FILE_ATTRIBUTES)
    {
        WCHAR e[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(e, ARRAYSIZE(e), L"WGConnect: wireguard.exe nicht gefunden: '%s' err=%lu",
                         pwszExePath, GetLastError());
        LOG_CRIT(e); return false;
    }

    WCHAR wszCmd[MAX_PATH_WGCP * 2] = {};
    StringCchPrintfW(wszCmd, ARRAYSIZE(wszCmd),
                     L"\"%s\" /installtunnelservice \"%s\"", pwszExePath, wszConfig);
    WCHAR d[MAX_PATH_WGCP * 2 + 32] = {};
    StringCchPrintfW(d, ARRAYSIZE(d), L"WGConnect: %s", wszCmd); LOG_DEBUG(d);

    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, wszCmd, nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"WGConnect: CreateProcess failed err=%lu", GetLastError());
        LOG_CRIT(e); return false;
    }
    WaitForSingleObject(pi.hProcess, 10000);
    DWORD dwExit = 0;
    GetExitCodeProcess(pi.hProcess, &dwExit);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);

    WCHAR r[64] = {};
    StringCchPrintfW(r, 64, L"WGConnect: wireguard.exe exit code=%lu", dwExit);
    if (dwExit == 0) LOG_DEBUG(r); else LOG_WARN(r);
    return dwExit == 0;
}

// ---------------------------------------------------------------------------
// Disconnect tunnel: wireguard.exe /uninstalltunnelservice <name>
// ---------------------------------------------------------------------------
inline bool WGDisconnect(PCWSTR pwszExePath, PCWSTR pwszProfile)
{
    WCHAR wszCmd[MAX_PATH_WGCP * 2] = {};
    StringCchPrintfW(wszCmd, ARRAYSIZE(wszCmd),
                     L"\"%s\" /uninstalltunnelservice %s", pwszExePath, pwszProfile);
    WCHAR d[MAX_PATH_WGCP * 2 + 32] = {};
    StringCchPrintfW(d, ARRAYSIZE(d), L"WGDisconnect: %s", wszCmd); LOG_DEBUG(d);

    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, wszCmd, nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"WGDisconnect: CreateProcess failed err=%lu", GetLastError());
        LOG_CRIT(e);
        return false;
    }
    WaitForSingleObject(pi.hProcess, 5000);
    DWORD dwExit = 0;
    GetExitCodeProcess(pi.hProcess, &dwExit);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    WCHAR r[64] = {};
    StringCchPrintfW(r, 64, L"WGDisconnect: exit code=%lu", dwExit);
    LOG_DEBUG(r);
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
// Connection timer: "⏱ Connected since HH:MM:SS"
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
        // Determine display language from system locale
        LANGID lid = GetUserDefaultUILanguage();
        bool bDE = (PRIMARYLANGID(lid) == LANG_GERMAN);
        if (!bDE) { lid = GetSystemDefaultUILanguage(); bDE = (PRIMARYLANGID(lid) == LANG_GERMAN); }
        if (!bDE) { lid = LANGIDFROMLCID(GetUserDefaultLCID()); bDE = (PRIMARYLANGID(lid) == LANG_GERMAN); }
        PCWSTR pwszLabel = bDE ? L"\u23F1 Verbunden seit" : L"\u23F1 Connected since";
        StringCchPrintfW(pwszOut, cchOut, L"%s %02d:%02d:%02d", pwszLabel, h, m, s);
    }
    CloseHandle(hProc);
}

// ---------------------------------------------------------------------------
// Smartcard configuration
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
    // Read thumbprint as plain REG_SZ.
    // Note: The thumbprint is not a secret - it only identifies which certificate
    // is accepted. DPAPI LocalMachine encryption cannot be used here because the
    // CP runs as SYSTEM on the pre-logon screen and SYSTEM cannot decrypt blobs
    // created by an interactive user session.
    ReadRegString(hKey, WGCP_REG_SC_CERT_THUMBPRINT,
                  cfg.wszCertThumbprint, 128, L"");
    if (cfg.wszCertThumbprint[0])
        LOG_DEBUG(L"SC-Config: thumbprint loaded from registry");

    RegCloseKey(hKey);

    // Log configuration
    WCHAR wszLog[512] = {};
    StringCchPrintfW(wszLog, ARRAYSIZE(wszLog),
        L"SC-Config: enabled=%d pinReq=%d pinMin=%lu maxAttempts=%lu timeout=%lu "
        L"connectOnInsert=%d disconnectOnRemove=%d reader='%s' thumbprint='%s'",
        cfg.bEnabled, cfg.bPinRequired, cfg.dwPinMinLength, cfg.dwPinMaxAttempts,
        cfg.dwTimeout, cfg.bConnectOnInsert, cfg.bDisconnectOnRemove,
        cfg.wszReaderName, cfg.wszCertThumbprint);
    LOG_DEBUG(wszLog);
}

// ---------------------------------------------------------------------------
// Smartcard / WinSCard helper functions
// ---------------------------------------------------------------------------
#include <winscard.h>
#pragma comment(lib, "winscard.lib")
#pragma comment(lib, "crypt32.lib")

// Result of a smartcard authentication attempt
enum class WGCPScResult
{
    Success,
    NoCard,
    WrongCard,       // Thumbprint does not match
    PinWrong,
    PinLocked,
    Timeout,
    Disabled,
    Error
};

// Checks whether a card is present and returns the reader name
inline bool WGCPFindSmartcard(const WGCPSmartcardConfig& cfg,
                               WCHAR* pwszReaderOut, DWORD cchReader)
{
    SCARDCONTEXT hCtx = 0;
    if (SCardEstablishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hCtx) != SCARD_S_SUCCESS)
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"SC: SCardEstablishContext err=%lu", GetLastError());
        LOG_WARN(e);
        return false;
    }

    // Use configured reader or search all readers
    if (cfg.wszReaderName[0] != L'\0')
    {
        WCHAR d[320] = {};
        StringCchPrintfW(d, 320, L"SC: Checking configured reader '%s'", cfg.wszReaderName);
        LOG_DEBUG(d);
        SCARD_READERSTATEW rs = {};
        rs.szReader     = cfg.wszReaderName;
        rs.dwCurrentState = SCARD_STATE_UNAWARE;
        LONG lRet = SCardGetStatusChangeW(hCtx, 0, &rs, 1);
        SCardReleaseContext(hCtx);
        if (lRet == SCARD_S_SUCCESS &&
            (rs.dwEventState & SCARD_STATE_PRESENT))
        {
            WCHAR d2[320] = {};
            StringCchPrintfW(d2, 320, L"SC: Card found in reader '%s'", cfg.wszReaderName);
            LOG_DEBUG(d2);
            StringCchCopyW(pwszReaderOut, cchReader, cfg.wszReaderName);
            return true;
        }
        LOG_DEBUG(L"SC: No card in configured reader");
        return false;
    }

    // Enumerate all readers
    DWORD dwLen = SCARD_AUTOALLOCATE;
    LPWSTR pwszReaders = nullptr;
    LONG lRet = SCardListReadersW(hCtx, nullptr,
                                   reinterpret_cast<LPWSTR>(&pwszReaders), &dwLen);
    if (lRet != SCARD_S_SUCCESS || !pwszReaders)
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"SC: SCardListReaders err=0x%08X", lRet);
        LOG_WARN(e);
        SCardReleaseContext(hCtx);
        return false;
    }

    // Log all available readers
    LOG_DEBUG(L"SC: Available readers:");
    for (LPCWSTR pLog = pwszReaders; *pLog; pLog += wcslen(pLog) + 1)
    {
        WCHAR dLog[320] = {};
        StringCchPrintfW(dLog, 320, L"SC:   -> '%s'", pLog);
        LOG_DEBUG(dLog);
    }

    bool bFound = false;
    for (LPCWSTR p = pwszReaders; *p; p += wcslen(p) + 1)
    {
        // Skip virtual SIM/UICC readers – they are not PIV-capable smartcard readers
        if (wcsstr(p, L"UICC") || wcsstr(p, L"SIM") || wcsstr(p, L"Microsoft UICC"))
        {
            WCHAR dSkip[320] = {};
            StringCchPrintfW(dSkip, 320, L"SC: Skipping virtual reader '%s'", p);
            LOG_DEBUG(dSkip);
            continue;
        }

        WCHAR d[320] = {};
        StringCchPrintfW(d, 320, L"SC: Checking reader '%s'", p);
        LOG_DEBUG(d);
        SCARD_READERSTATEW rs = {};
        rs.szReader      = p;
        rs.dwCurrentState = SCARD_STATE_UNAWARE;
        if (SCardGetStatusChangeW(hCtx, 0, &rs, 1) == SCARD_S_SUCCESS &&
            (rs.dwEventState & SCARD_STATE_PRESENT))
        {
            WCHAR d2[320] = {};
            StringCchPrintfW(d2, 320, L"SC: Card found in reader '%s'", p);
            LOG_DEBUG(d2);
            StringCchCopyW(pwszReaderOut, cchReader, p);
            bFound = true;
            break;
        }
    }

    if (!bFound) LOG_DEBUG(L"SC: No card found in any reader");
    SCardFreeMemory(hCtx, pwszReaders);
    SCardReleaseContext(hCtx);
    return bFound;
}

// Waits until a card is inserted (timeout in seconds, 0 = immediate)
inline bool WGCPWaitForCard(const WGCPSmartcardConfig& cfg,
                             WCHAR* pwszReaderOut, DWORD cchReader)
{
    WCHAR d[64] = {};
    StringCchPrintfW(d, 64, L"SC: Waiting for card (timeout %lu s)...", cfg.dwTimeout);
    LOG_DEBUG(d);
    DWORD dwDeadline = GetTickCount() + cfg.dwTimeout * 1000;
    do {
        if (WGCPFindSmartcard(cfg, pwszReaderOut, cchReader))
            return true;
        Sleep(500);
    } while (GetTickCount() < dwDeadline);
    LOG_WARN(L"SC: Timeout - no card found");
    return false;
}

// Checks whether the card has been removed
inline bool WGCPIsCardRemoved(PCWSTR pwszReader)
{
    SCARDCONTEXT hCtx = 0;
    if (SCardEstablishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hCtx) != SCARD_S_SUCCESS)
    {
        LOG_WARN(L"SC: SCardEstablishContext (IsCardRemoved) failed");
        return true;
    }

    SCARD_READERSTATEW rs = {};
    rs.szReader      = pwszReader;
    rs.dwCurrentState = SCARD_STATE_UNAWARE;
    LONG lRet = SCardGetStatusChangeW(hCtx, 0, &rs, 1);
    SCardReleaseContext(hCtx);

    if (lRet != SCARD_S_SUCCESS) return true;
    bool bRemoved = (rs.dwEventState & SCARD_STATE_EMPTY) != 0;
    if (bRemoved)
    {
        WCHAR d[320] = {};
        StringCchPrintfW(d, 320, L"SC: Card removed from reader '%s'", pwszReader);
        LOG_DEBUG(d);
    }
    return bRemoved;
}

// Verifies certificate thumbprint on the card (empty = no check)
inline bool WGCPVerifyCertThumbprint(SCARDHANDLE hCard, PCWSTR pwszExpected)
{
    if (!pwszExpected || pwszExpected[0] == L'\0')
    {
        LOG_DEBUG(L"SC: No thumbprint configured - certificate check skipped");
        return true;
    }
    WCHAR d[160] = {};
    StringCchPrintfW(d, 160, L"SC: Verifying certificate thumbprint '%s'", pwszExpected);
    LOG_DEBUG(d);

    // Read ATR and verify certificate via CryptoAPI
    // Open card as smartcard store
    // Helper lambda: search one store for the expected thumbprint
    auto SearchStore = [&](DWORD dwFlags, PCWSTR pwszStore) -> bool
    {
        HCERTSTORE hStore = CertOpenStore(
            CERT_STORE_PROV_SYSTEM_W, 0, 0,
            dwFlags | CERT_STORE_READONLY_FLAG,
            pwszStore);
        if (!hStore) return false;

        PCCERT_CONTEXT pCert = nullptr;
        while ((pCert = CertEnumCertificatesInStore(hStore, pCert)) != nullptr)
        {
            BYTE  rgThumb[20] = {};
            DWORD cbThumb     = sizeof(rgThumb);
            if (!CertGetCertificateContextProperty(pCert, CERT_SHA1_HASH_PROP_ID,
                                                   rgThumb, &cbThumb))
                continue;

            WCHAR wszThumb[48] = {};
            for (DWORD i = 0; i < cbThumb; i++)
                StringCchPrintfW(wszThumb + i*2, 3, L"%02X", rgThumb[i]);

            if (_wcsicmp(wszThumb, pwszExpected) == 0)
            {
                CertFreeCertificateContext(pCert);
                CertCloseStore(hStore, 0);
                return true;
            }
        }
        CertCloseStore(hStore, 0);
        return false;
    };

    // 1. Read certificate directly from PIV slot 9a via GET DATA APDU.
    //    Works in pre-logon (SYSTEM) and post-logon without depending on
    //    any Windows certificate store or user context.
    bool bMatch = false;
    {
        // PIV GET DATA for Certificate in slot 9a (no Le - use GET RESPONSE chaining)
        BYTE apduGetCert[] = {
            0x00, 0xCB, 0x3F, 0xFF, 0x05,
            0x5C, 0x03, 0x5F, 0xC1, 0x05
        };
        BYTE   certBuf[8192] = {};
        DWORD  dwCertRecv    = 0;
        // YubiKey always uses T=1 protocol
        const SCARD_IO_REQUEST* pPci = SCARD_PCI_T1;

        // SELECT PIV Application first (required before any PIV data command)
        BYTE selectAid[] = {
            0x00, 0xA4, 0x04, 0x00, 0x0B,
            0xA0, 0x00, 0x00, 0x03, 0x08, 0x00, 0x00, 0x10, 0x00, 0x01, 0x00
        };
        BYTE  selResp[512] = {}; DWORD dwSelRecv = sizeof(selResp);
        SCardTransmit(hCard, pPci, selectAid, sizeof(selectAid),
                      nullptr, selResp, &dwSelRecv);

        // GET DATA with GET RESPONSE chaining for large responses (SW=61xx)
        LONG lGet = SCARD_F_INTERNAL_ERROR;
        for (int iTry = 0; iTry < 3; iTry++)
        {
            ZeroMemory(certBuf, sizeof(certBuf));
            dwCertRecv = 0;
            BYTE tmpBuf[512] = {};
            DWORD dwTmp = sizeof(tmpBuf);
            lGet = SCardTransmit(hCard, pPci, apduGetCert, sizeof(apduGetCert),
                                 nullptr, tmpBuf, &dwTmp);
            if (lGet != SCARD_S_SUCCESS) { Sleep(100); continue; }

            // Copy data bytes (exclude trailing SW1 SW2)
            DWORD dwData = (dwTmp >= 2) ? dwTmp - 2 : 0;
            if (dwData > 0 && dwData <= sizeof(certBuf))
                memcpy(certBuf, tmpBuf, dwData);
            dwCertRecv = dwData;

            // Chain: SW=61xx means more data available - send GET RESPONSE
            while (dwTmp >= 2 && tmpBuf[dwTmp-2] == 0x61)
            {
                BYTE remaining = tmpBuf[dwTmp-1];
                BYTE apduGetResp[] = { 0x00, 0xC0, 0x00, 0x00, remaining };
                ZeroMemory(tmpBuf, sizeof(tmpBuf));
                dwTmp = sizeof(tmpBuf);
                lGet = SCardTransmit(hCard, pPci, apduGetResp, sizeof(apduGetResp),
                                     nullptr, tmpBuf, &dwTmp);
                if (lGet != SCARD_S_SUCCESS) break;
                dwData = (dwTmp >= 2) ? dwTmp - 2 : 0;
                if (dwCertRecv + dwData <= sizeof(certBuf))
                {
                    memcpy(certBuf + dwCertRecv, tmpBuf, dwData);
                    dwCertRecv += dwData;
                }
            }
            break;
        }
        // Log GET DATA result
        {
            SYSTEMTIME stG={}; GetLocalTime(&stG);
            WCHAR wLG[MAX_PATH]={};
            StringCchPrintfW(wLG,MAX_PATH,L"C:\\Windows\\Temp\\wgcp_%02d%02d%04d_cp.log",
                stG.wDay,stG.wMonth,stG.wYear);
            HANDLE hLG=CreateFileW(wLG,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,
                nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(hLG!=INVALID_HANDLE_VALUE){
                char szG[128]={};
                wsprintfA(szG,"[%02d:%02d:%02d] SC: GET_DATA lRet=0x%08X recv=%lu b0=%02X b1=%02X b2=%02X b3=%02X b4=%02X\r\n",
                    stG.wHour,stG.wMinute,stG.wSecond,(unsigned)lGet,(unsigned long)dwCertRecv,
                    dwCertRecv>0?certBuf[0]:0, dwCertRecv>1?certBuf[1]:0,
                    dwCertRecv>2?certBuf[2]:0, dwCertRecv>3?certBuf[3]:0,
                    dwCertRecv>4?certBuf[4]:0);
                DWORD dw=0; WriteFile(hLG,szG,lstrlenA(szG),&dw,nullptr);
                CloseHandle(hLG);
            }
        }
        if (lGet == SCARD_S_SUCCESS && dwCertRecv > 4)
        {
            // Response contains DER-encoded certificate (after TLV header)
            // Find the certificate DER data: look for 0x70 tag
            BYTE* pDer = certBuf;
            DWORD cbDer = dwCertRecv - 2; // strip SW bytes

            // Skip outer TLV (0x53 tag)
            if (cbDer > 4 && pDer[0] == 0x53)
            {
                DWORD skip = 2;
                if (pDer[1] & 0x80) skip += (pDer[1] & 0x7F);
                pDer += skip; cbDer -= skip;
            }
            // Skip 0x70 certificate tag
            if (cbDer > 4 && pDer[0] == 0x70)
            {
                DWORD len = 0;
                DWORD skip = 2;
                if (pDer[1] & 0x80)
                {
                    DWORD nb = pDer[1] & 0x7F;
                    for (DWORD i = 0; i < nb; i++)
                        len = (len << 8) | pDer[2 + i];
                    skip = 2 + nb;
                }
                else len = pDer[1];
                pDer += skip; cbDer = len;
            }

            if (cbDer > 0)
            {
                PCCERT_CONTEXT pCtx = CertCreateCertificateContext(
                    X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, pDer, cbDer);
                if (pCtx)
                {
                    BYTE  rgThumb[20] = {};
                    DWORD cbThumb     = sizeof(rgThumb);
                    if (CertGetCertificateContextProperty(
                            pCtx, CERT_SHA1_HASH_PROP_ID, rgThumb, &cbThumb))
                    {
                        WCHAR wszThumb[48] = {};
                        for (DWORD i = 0; i < cbThumb; i++)
                            StringCchPrintfW(wszThumb + i*2, 3, L"%02X", rgThumb[i]);
                        bMatch = (_wcsicmp(wszThumb, pwszExpected) == 0);
                        if (bMatch)
                            LOG_DEBUG(L"SC: Thumbprint matched via PIV GET DATA");
                        else
                        {
                            WCHAR eT[160] = {};
                            StringCchPrintfW(eT, 160,
                                L"SC: PIV cert thumbprint=%s expected=%s",
                                wszThumb, pwszExpected);
                            LOG_WARN(eT);
                        }
                    }
                    CertFreeCertificateContext(pCtx);
                }
            }
        }
        else
        {
            WCHAR eG[64] = {};
            StringCchPrintfW(eG, 64, L"SC: GET DATA failed lRet=0x%08X", lGet);
            LOG_WARN(eG);
        }
    }

    // 2. Fallback: Current User MY store (post-logon, YubiKey Minidriver)
    if (!bMatch)
        bMatch = SearchStore(CERT_SYSTEM_STORE_CURRENT_USER, L"MY");

    // 3. Fallback: Local Machine MY store
    if (!bMatch)
        bMatch = SearchStore(CERT_SYSTEM_STORE_LOCAL_MACHINE, L"MY");

    if (bMatch)
        LOG_DEBUG(L"SC: Thumbprint verification successful");
    else
        LOG_WARN(L"SC: No matching certificate found");
    return bMatch;
}

// Main authentication function
// pwszPin: PIN (may be nullptr if bPinRequired=false)
inline WGCPScResult WGCPAuthenticateSmartcard(const WGCPSmartcardConfig& cfg,
                                               PCWSTR pwszPin)
{
    if (!cfg.bEnabled) return WGCPScResult::Disabled;

    // Find reader
    WCHAR wszReader[256] = {};
    if (!WGCPWaitForCard(cfg, wszReader, 256))
    {
        LOG_WARN(L"Smartcard: No card found (timeout)");
        return WGCPScResult::Timeout;
    }

    WCHAR d[512] = {};
    StringCchPrintfW(d, 512, L"Smartcard: Card found in reader '%s'", wszReader);
    LOG_DEBUG(d);

    // Connect to card
    SCARDCONTEXT hCtx   = 0;
    SCARDHANDLE  hCard  = 0;
    DWORD        dwProto = 0;

    if (SCardEstablishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &hCtx) != SCARD_S_SUCCESS)
        return WGCPScResult::Error;

    LONG lRet = SCardConnectW(hCtx, wszReader,
                               SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
                               &hCard, &dwProto);
    if (lRet == SCARD_S_SUCCESS)
    {
        WCHAR eP[64] = {};
        StringCchPrintfW(eP, 64, L"Smartcard: Connected proto=%s",
                         dwProto == SCARD_PROTOCOL_T0 ? L"T=0" : L"T=1");
        LOG_DEBUG(eP);
    }
    if (lRet != SCARD_S_SUCCESS)
    {
        WCHAR eC[96] = {};
        StringCchPrintfW(eC, 96,
            L"Smartcard: SCardConnect failed lRet=0x%08X reader='%s'", lRet, wszReader);
        LOG_WARN(eC);
        SCardReleaseContext(hCtx);
        return WGCPScResult::Error;
    }

    if (!WGCPVerifyCertThumbprint(hCard, cfg.wszCertThumbprint))
    {
        SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        SCardReleaseContext(hCtx);
        LOG_WARN(L"Smartcard: Certificate thumbprint mismatch");
        return WGCPScResult::WrongCard;
    }

    // PIN verification via VERIFY APDU (ISO 7816-4)
    if (cfg.bPinRequired && pwszPin && pwszPin[0] != L'\0')
    {
        // Convert PIN from Unicode to ASCII
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
        // Data: PIN padded with 0xFF to 8 bytes
        BYTE apdu[13] = { 0x00, 0x20, 0x00, 0x80, 0x08,
                          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        memcpy(apdu + 5, szPin, (dwPinLen < 8u ? dwPinLen : 8u));

        // Response buffer: 258 bytes (max APDU response + 2 SW bytes).
        // A 2-byte buffer causes ERROR_INVALID_PARAMETER (0x57) on some readers.
        BYTE   resp[258] = {};
        DWORD  dwRecv    = sizeof(resp);
        const SCARD_IO_REQUEST* pProto = (dwProto == SCARD_PROTOCOL_T0)
                                       ? SCARD_PCI_T0 : SCARD_PCI_T1;

        // SELECT PIV Application before VERIFY
        // Required for NFC readers (Microsoft UICC) and some USB readers.
        // AID: A0 00 00 03 08 00 00 10 00 01 00 (NIST PIV)
        BYTE selectApdu[] = {
            0x00, 0xA4, 0x04, 0x00, 0x0B,
            0xA0, 0x00, 0x00, 0x03, 0x08, 0x00, 0x00, 0x10, 0x00, 0x01, 0x00
        };
        BYTE   selResp[258] = {};
        DWORD  dwSelRecv    = sizeof(selResp);
        LONG   lSel = SCardTransmit(hCard, pProto, selectApdu, sizeof(selectApdu),
                                    nullptr, selResp, &dwSelRecv);
        WCHAR eSel[96] = {};
        StringCchPrintfW(eSel, 96,
            L"Smartcard: SELECT PIV lRet=0x%08X SW=%02X%02X",
            lSel, dwSelRecv >= 2 ? selResp[dwSelRecv-2] : 0,
                  dwSelRecv >= 1 ? selResp[dwSelRecv-1] : 0);
        LOG_DEBUG(eSel);

        WCHAR eA[64] = {};
        StringCchPrintfW(eA, 64, L"Smartcard: Sending VERIFY APDU (pinLen=%lu)", dwPinLen);
        LOG_DEBUG(eA);

        lRet = SCardTransmit(hCard, pProto, apdu, sizeof(apdu),
                             nullptr, resp, &dwRecv);

        // Securely erase PIN from memory
        SecureZeroMemory(szPin, sizeof(szPin));
        SecureZeroMemory(apdu + 5, 8);

        if (lRet != SCARD_S_SUCCESS)
        {
            WCHAR eT[96] = {};
            StringCchPrintfW(eT, 96,
                L"Smartcard: SCardTransmit failed lRet=0x%08X", lRet);
            LOG_WARN(eT);
            SCardDisconnect(hCard, SCARD_LEAVE_CARD);
            SCardReleaseContext(hCtx);
            return WGCPScResult::Error;
        }

        // SW1=90, SW2=00 -> success
        // SW1=63, SW2=CX -> X attempts remaining
        // SW1=69, SW2=83 -> PIN locked
        WCHAR eR[64] = {};
        StringCchPrintfW(eR, 64, L"Smartcard: APDU response SW1=0x%02X SW2=0x%02X", resp[0], resp[1]);
        LOG_DEBUG(eR);

        if (resp[0] == 0x90 && resp[1] == 0x00)
        {
            LOG_DEBUG(L"Smartcard: PIN verification successful");
        }
        else if (resp[0] == 0x69 && resp[1] == 0x83)
        {
            SCardDisconnect(hCard, SCARD_LEAVE_CARD);
            SCardReleaseContext(hCtx);
            LOG_WARN(L"Smartcard: PIN locked");
            return WGCPScResult::PinLocked;
        }
        else
        {
            WCHAR e[64] = {};
            DWORD remaining = resp[1] & 0x0F;
            StringCchPrintfW(e, 64,
                             L"Smartcard: Wrong PIN. Remaining attempts: %lu", remaining);
            LOG_WARN(e);
            SCardDisconnect(hCard, SCARD_LEAVE_CARD);
            SCardReleaseContext(hCtx);
            return WGCPScResult::PinWrong;
        }
    }

    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    SCardReleaseContext(hCtx);
    LOG_DEBUG(L"Smartcard: Authentication successful");
    return WGCPScResult::Success;
}
