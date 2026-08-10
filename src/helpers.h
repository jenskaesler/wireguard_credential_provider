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
#include <wincred.h>   // CredUIPromptForCredentialsW
#include <netlistmgr.h> // INetworkListManager (NLA)
#include <ws2tcpip.h>    // AF_UNSPEC, sockaddr
#include <iphlpapi.h>    // GetAdaptersAddresses, IP_ADAPTER_ADDRESSES
#include <comdef.h>     // _com_ptr_t  // DATA_BLOB, CryptProtectData, CryptUnprotectData
#include <dpapi.h>     // CRYPTPROTECT_LOCAL_MACHINE
#include <lmcons.h>    // DNLEN, required before dsgetdc.h
#include <dsgetdc.h>   // DsGetDcNameW, DOMAIN_CONTROLLER_INFOW, DS_FORCE_REDISCOVERY

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ws2_32.lib")

#ifndef WGCP_TRAY_BUILD
  // CP DLL only
  #include <shlobj.h>
  #include <credentialprovider.h>
  #include <shlwapi.h>
  #include "../resources/resource.h"
  #pragma comment(lib, "shlwapi.lib")
  #pragma comment(lib, "credui.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "oleaut32.lib")
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
#pragma comment(lib, "iphlpapi.lib")
  #pragma comment(lib, "shell32.lib")
#endif

// ---------------------------------------------------------------------------
// Registry key and values
// ---------------------------------------------------------------------------
#define WGCP_REG_KEY          L"SOFTWARE\\Jens Kaesler\\WireGuard Credential Provider"
#define WGCP_REG_EXEPATH      L"ExePath"
#define WGCP_REG_WGEXEPATH              L"WgExePath"
#define WGCP_REG_HANDSHAKE_TIMEOUT_SEC  L"HandshakeTimeoutSec"
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
    DWORD dwType = 0, dwVal = 0, cbData = sizeof(dwVal);
    // Try REG_DWORD first
    if (RegQueryValueExW(hKey, pwszValue, nullptr, &dwType,
                         reinterpret_cast<LPBYTE>(&dwVal), &cbData) == ERROR_SUCCESS
        && dwType == REG_DWORD)
        return dwVal;
    // Fallback: REG_SZ with decimal number (easier to edit without hex conversion)
    WCHAR wszStr[32] = {};
    DWORD cbStr = sizeof(wszStr);
    dwType = 0;
    if (RegQueryValueExW(hKey, pwszValue, nullptr, &dwType,
                         reinterpret_cast<LPBYTE>(wszStr), &cbStr) == ERROR_SUCCESS
        && (dwType == REG_SZ || dwType == REG_EXPAND_SZ) && wszStr[0])
        return static_cast<DWORD>(_wtoi(wszStr));
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
    {
        // Emergency fallback – wird im Normalfall nie erreicht.
        // Kein LOG_WARN hier da WGCPLog/LOG_WARN erst nach dieser Funktion
        // definiert wird. Der Fallback-Pfad wird durch das spätere Logging
        // in den aufrufenden Funktionen sichtbar.
        StringCchCopyW(pwszOut, cchOut, L"C:\\Windows\\Temp\\wgcp_configurations");
    }

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
            // No InstallDir known - no log path available
            pwszOut[0] = L'\0';
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
            SHCreateDirectoryExW(nullptr, wszDir, nullptr);
        }
    }

    // Open or create log file
    HANDLE hFile = CreateFileW(wszPath, FILE_APPEND_DATA,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

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
// Network Location Awareness
// Returns true when the machine has an active domain-authenticated network
// connection – i.e. it is physically inside the corporate network.
//
// Two-stage detection:
//   Stage 1 (Primary): NLA reports DOMAIN_AUTHENTICATED for the network.
//   Stage 2 (Fallback): PC is domain-joined AND DsGetDcName finds a DC on the
//     LAN – catches the common VM case (Red Hat VirtIO, Hyper-V) where NLA
//     only reports PRIVATE instead of DOMAIN_AUTHENTICATED because the DC was
//     not reachable when NLA first classified the network after boot.
//
// WireGuard interfaces are excluded from both stages.
// ---------------------------------------------------------------------------
inline bool WGCPIsOnCorporateNetwork()
{
    HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool bNeedCoUninit = (SUCCEEDED(hrCo) && hrCo != S_FALSE);
    if (hrCo == RPC_E_CHANGED_MODE) bNeedCoUninit = false;

    bool bCorporate = false;

    INetworkListManager* pNLM = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_NetworkListManager, nullptr,
                                  CLSCTX_ALL, IID_INetworkListManager,
                                  reinterpret_cast<void**>(&pNLM));
    if (FAILED(hr) || !pNLM)
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"CorpNet: CoCreateInstance(NLM) failed hr=0x%08X", hr);
        LOG_WARN(e);
        if (bNeedCoUninit) CoUninitialize();
        return false;
    }

    // Build WireGuard adapter GUID set (used by both stages)
    WCHAR wszWgGuids[64][64] = {};
    int   nWgGuids = 0;
    {
        ULONG ulSize = 0;
        GetAdaptersAddresses(AF_UNSPEC,
            GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST,
            nullptr, nullptr, &ulSize);
        if (ulSize > 0)
        {
            IP_ADAPTER_ADDRESSES* pAddrs =
                static_cast<IP_ADAPTER_ADDRESSES*>(malloc(ulSize));
            if (pAddrs)
            {
                ULONG ulRet = GetAdaptersAddresses(AF_UNSPEC,
                    GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST,
                    nullptr, pAddrs, &ulSize);
                if (ulRet == NO_ERROR)
                {
                    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
                    for (IP_ADAPTER_ADDRESSES* p = pAddrs; p && nWgGuids < 64; p = p->Next)
                    {
                        bool bIsWG = false;
                        if (hSCM && p->FriendlyName && p->FriendlyName[0])
                        {
                            WCHAR wszSvc[300] = {};
                            StringCchPrintfW(wszSvc, ARRAYSIZE(wszSvc),
                                L"WireGuardTunnel$%s", p->FriendlyName);
                            SC_HANDLE hSvc = OpenServiceW(hSCM, wszSvc, SERVICE_QUERY_STATUS);
                            if (hSvc) { bIsWG = true; CloseServiceHandle(hSvc); }
                        }
                        if (!bIsWG && p->Description &&
                            wcsstr(p->Description, L"WireGuard") != nullptr)
                            bIsWG = true;
                        if (bIsWG)
                        {
                            WCHAR wszGuid[64] = {};
                            MultiByteToWideChar(CP_ACP, 0, p->AdapterName, -1, wszGuid, 64);
                            StringCchCopyW(wszWgGuids[nWgGuids++], 64, wszGuid);
                            WCHAR d[128] = {};
                            StringCchPrintfW(d, ARRAYSIZE(d),
                                L"CorpNet: WireGuard adapter identified: '%s'",
                                p->FriendlyName ? p->FriendlyName : L"(unknown)");
                            LOG_DEBUG(d);
                        }
                    }
                    if (hSCM) CloseServiceHandle(hSCM);
                }
                free(pAddrs);
            }
        }
    }

    // -----------------------------------------------------------------------
    // Stage 1: NLA category check (DOMAIN_AUTHENTICATED)
    // -----------------------------------------------------------------------
    IEnumNetworks* pEnum = nullptr;
    if (SUCCEEDED(pNLM->GetNetworks(NLM_ENUM_NETWORK_CONNECTED, &pEnum)) && pEnum)
    {
        INetwork* pNet = nullptr;
        while (pEnum->Next(1, &pNet, nullptr) == S_OK && !bCorporate)
        {
            NLM_NETWORK_CATEGORY cat = NLM_NETWORK_CATEGORY_PUBLIC;
            if (FAILED(pNet->GetCategory(&cat)) ||
                cat != NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED)
            {
                pNet->Release();
                continue;
            }
            bool bAllWireGuard = true;
            bool bHasConnections = false;
            IEnumNetworkConnections* pConnEnum = nullptr;
            if (SUCCEEDED(pNet->GetNetworkConnections(&pConnEnum)) && pConnEnum)
            {
                INetworkConnection* pConn = nullptr;
                while (pConnEnum->Next(1, &pConn, nullptr) == S_OK)
                {
                    bHasConnections = true;
                    GUID adapterGuid = {};
                    bool bThisIsWG = false;
                    if (SUCCEEDED(pConn->GetAdapterId(&adapterGuid)) && nWgGuids > 0)
                    {
                        WCHAR wszGuid[64] = {};
                        StringFromGUID2(adapterGuid, wszGuid, ARRAYSIZE(wszGuid));
                        for (int g = 0; g < nWgGuids; g++)
                        {
                            if (_wcsicmp(wszGuid, wszWgGuids[g]) == 0)
                            { bThisIsWG = true; break; }
                        }
                    }
                    if (!bThisIsWG) bAllWireGuard = false;
                    pConn->Release();
                }
                pConnEnum->Release();
            }
            if (bHasConnections && !bAllWireGuard)
            {
                NLM_NETWORK_CATEGORY catCheck = NLM_NETWORK_CATEGORY_PUBLIC;
                if (SUCCEEDED(pNet->GetCategory(&catCheck)) &&
                    catCheck == NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED)
                {
                    bCorporate = true;
                    LOG_DEBUG(L"CorpNet: Stage1 - DOMAIN_AUTHENTICATED non-WireGuard network confirmed");
                }
                else
                    LOG_DEBUG(L"CorpNet: Stage1 - category changed during recheck, skipping");
            }
            else if (bHasConnections && bAllWireGuard)
                LOG_DEBUG(L"CorpNet: Stage1 - domain-authenticated but all WireGuard adapters, excluded");
            pNet->Release();
        }
        pEnum->Release();
    }
    else
        LOG_WARN(L"CorpNet: Stage1 - GetNetworks failed");

    // -----------------------------------------------------------------------
    // Stage 2: Fallback for domain-joined PCs where NLA shows PRIVATE.
    //
    // Root cause (seen with Red Hat VirtIO / Hyper-V adapters):
    //   NLA classifies the network as PRIVATE because it could not verify
    //   domain controller reachability at boot time. The classification is
    //   then cached and not updated even after the DC becomes reachable.
    //
    // Fix: if Stage 1 missed, check:
    //   (a) PC is domain-joined (registry SYSTEM\...\Tcpip\Parameters\Domain)
    //   (b) At least one non-WireGuard LAN adapter is up with an IPv4 address
    //   (c) DsGetDcName succeeds – this does a real DNS/NetBIOS query for a DC
    //       on the current network. If it finds one, the LAN has DC access.
    // -----------------------------------------------------------------------
    if (!bCorporate)
    {
        // (a) Domain-join check via registry
        bool bDomainJoined = false;
        WCHAR wszDomainName[256] = {};
        {
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                0, KEY_READ, &hKey) == ERROR_SUCCESS)
            {
                DWORD cbData = sizeof(wszDomainName);
                DWORD dwType = 0;
                if (RegQueryValueExW(hKey, L"Domain", nullptr, &dwType,
                    reinterpret_cast<LPBYTE>(wszDomainName), &cbData) == ERROR_SUCCESS
                    && dwType == REG_SZ && wszDomainName[0] != L'\0')
                    bDomainJoined = true;
                RegCloseKey(hKey);
            }
        }

        if (!bDomainJoined)
        {
            LOG_DEBUG(L"CorpNet: Stage2 - PC is not domain-joined, skipping");
        }
        else
        {
            WCHAR d[320] = {};
            StringCchPrintfW(d, ARRAYSIZE(d),
                L"CorpNet: Stage2 - PC is domain-joined (domain='%s'), checking LAN...", wszDomainName);
            LOG_DEBUG(d);

            // (b) Non-WireGuard LAN adapter with IPv4 address
            bool bHasNonWgLan = false;
            {
                ULONG ulSize2 = 0;
                GetAdaptersAddresses(AF_INET,
                    GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST,
                    nullptr, nullptr, &ulSize2);
                if (ulSize2 > 0)
                {
                    IP_ADAPTER_ADDRESSES* pAddrs2 =
                        static_cast<IP_ADAPTER_ADDRESSES*>(malloc(ulSize2));
                    if (pAddrs2)
                    {
                        if (GetAdaptersAddresses(AF_INET,
                            GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST,
                            nullptr, pAddrs2, &ulSize2) == NO_ERROR)
                        {
                            for (IP_ADAPTER_ADDRESSES* p = pAddrs2; p && !bHasNonWgLan; p = p->Next)
                            {
                                if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
                                if (p->IfType == IF_TYPE_TUNNEL) continue;
                                if (p->OperStatus != IfOperStatusUp) continue;
                                if (!p->FirstUnicastAddress) continue;
                                WCHAR wszGuid2[64] = {};
                                MultiByteToWideChar(CP_ACP, 0, p->AdapterName, -1, wszGuid2, 64);
                                bool bIsWG = false;
                                for (int g = 0; g < nWgGuids; g++)
                                    if (_wcsicmp(wszGuid2, wszWgGuids[g]) == 0) { bIsWG = true; break; }
                                if (!bIsWG)
                                {
                                    bHasNonWgLan = true;
                                    WCHAR d2[320] = {};
                                    StringCchPrintfW(d2, ARRAYSIZE(d2),
                                        L"CorpNet: Stage2 - non-WireGuard LAN adapter up: '%s'",
                                        p->FriendlyName ? p->FriendlyName : L"(unknown)");
                                    LOG_DEBUG(d2);
                                }
                            }
                        }
                        free(pAddrs2);
                    }
                }
            }

            if (!bHasNonWgLan)
            {
                LOG_DEBUG(L"CorpNet: Stage2 - no non-WireGuard LAN adapter up, skipping DC lookup");
            }
            else
            {
                // (c) DsGetDcName: locate a DC on the network (dynamic load).
                // Types are from <dsgetdc.h> + <lmcons.h> (included at top of file).
                // We load dynamically so Netapi32.lib is not a link-time dependency.
                HMODULE hNetApi = LoadLibraryW(L"Netapi32.dll");
                if (hNetApi)
                {
                    typedef DWORD (WINAPI* PfnDsGetDcName)(
                        LPCWSTR ComputerName,
                        LPCWSTR DomainName,
                        GUID*   DomainGuid,
                        LPCWSTR SiteName,
                        ULONG   Flags,
                        PDOMAIN_CONTROLLER_INFOW* DomainControllerInfo);
                    typedef NET_API_STATUS (WINAPI* PfnNetApiBufferFree)(LPVOID);

                    PfnDsGetDcName  pfnDs   = reinterpret_cast<PfnDsGetDcName>(
                        GetProcAddress(hNetApi, "DsGetDcNameW"));
                    PfnNetApiBufferFree pfnFree = reinterpret_cast<PfnNetApiBufferFree>(
                        GetProcAddress(hNetApi, "NetApiBufferFree"));

                    if (pfnDs && pfnFree)
                    {
                        DOMAIN_CONTROLLER_INFOW* pDcInfo = nullptr;
                        // DS_FORCE_REDISCOVERY bypasses NLA/cache, does a real DNS query.
                        // DS_IP_REQUIRED ensures the DC has a reachable IP.
                        DWORD dwFlags = DS_FORCE_REDISCOVERY | DS_RETURN_DNS_NAME | DS_IP_REQUIRED;
                        DWORD dwErr = pfnDs(nullptr, nullptr, nullptr, nullptr,
                                           dwFlags, &pDcInfo);
                        if (dwErr == ERROR_SUCCESS && pDcInfo)
                        {
                            // DC was found – but it might be reachable only via WireGuard
                            // (Homeoffice VPN scenario). We must verify that the route to
                            // the DC's IP goes through a non-WireGuard adapter.
                            // If the DC address is only reachable via WireGuard, this is
                            // NOT a corporate LAN – it is a VPN-connected remote machine.
                            bool bDcViaLan = false;

                            // Parse DC IP from pDcInfo->DomainControllerAddress ("\\1.2.3.4")
                            PCWSTR pwszDcAddr = pDcInfo->DomainControllerAddress;
                            if (pwszDcAddr)
                            {
                                // DomainControllerAddress format: "\\1.2.3.4" (skip leading \\)
                                while (*pwszDcAddr == L'\\') pwszDcAddr++;

                                // Convert wide DC address to narrow for inet_pton
                                char szDcAddrA[64] = {};
                                WideCharToMultiByte(CP_ACP, 0, pwszDcAddr, -1,
                                    szDcAddrA, sizeof(szDcAddrA), nullptr, nullptr);

                                DWORD dwDcIp = 0;
                                if (inet_pton(AF_INET, szDcAddrA,
                                    reinterpret_cast<void*>(&dwDcIp)) == 1)
                                {
                                    // GetBestRoute: which interface does Windows use
                                    // to reach the DC IP?
                                    MIB_IPFORWARDROW route = {};
                                    if (GetBestRoute(dwDcIp, 0, &route) == NO_ERROR)
                                    {
                                        // route.dwForwardIfIndex = interface index for DC route
                                        // Compare against WireGuard adapter indices
                                        DWORD dwRouteIf = route.dwForwardIfIndex;

                                        // Collect WireGuard adapter interface indices
                                        ULONG ulSz3 = 0;
                                        GetAdaptersAddresses(AF_INET,
                                            GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST,
                                            nullptr, nullptr, &ulSz3);
                                        bool bRouteIsWG = false;
                                        if (ulSz3 > 0)
                                        {
                                            IP_ADAPTER_ADDRESSES* pA3 =
                                                static_cast<IP_ADAPTER_ADDRESSES*>(malloc(ulSz3));
                                            if (pA3)
                                            {
                                                if (GetAdaptersAddresses(AF_INET,
                                                    GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_MULTICAST,
                                                    nullptr, pA3, &ulSz3) == NO_ERROR)
                                                {
                                                    for (IP_ADAPTER_ADDRESSES* pA = pA3; pA; pA = pA->Next)
                                                    {
                                                        if (pA->IfIndex != dwRouteIf) continue;
                                                        // This is the adapter used to reach the DC
                                                        WCHAR wszGuid3[64] = {};
                                                        MultiByteToWideChar(CP_ACP, 0,
                                                            pA->AdapterName, -1, wszGuid3, 64);
                                                        for (int g = 0; g < nWgGuids; g++)
                                                        {
                                                            if (_wcsicmp(wszGuid3, wszWgGuids[g]) == 0)
                                                            { bRouteIsWG = true; break; }
                                                        }
                                                        WCHAR d4[320] = {};
                                                        StringCchPrintfW(d4, ARRAYSIZE(d4),
                                                            L"CorpNet: Stage2 - route to DC %s via adapter '%s' (isWG=%d)",
                                                            pwszDcAddr,
                                                            pA->FriendlyName ? pA->FriendlyName : L"?",
                                                            (int)bRouteIsWG);
                                                        LOG_DEBUG(d4);
                                                        break;
                                                    }
                                                }
                                                free(pA3);
                                            }
                                        }

                                        // Accept as corporate LAN only if the route to
                                        // the DC does NOT go through a WireGuard adapter
                                        bDcViaLan = !bRouteIsWG;
                                    }
                                    else
                                    {
                                        WCHAR d4[128] = {};
                                        StringCchPrintfW(d4, ARRAYSIZE(d4),
                                            L"CorpNet: Stage2 - GetBestRoute failed for DC %s", pwszDcAddr);
                                        LOG_WARN(d4);
                                        // Cannot determine route – assume NOT corporate to be safe
                                        bDcViaLan = false;
                                    }
                                }
                                else
                                {
                                    // DC address is not IPv4 (could be IPv6 or NetBIOS name)
                                    // Fall back to trusting DsGetDcName result without route check
                                    WCHAR d4[256] = {};
                                    StringCchPrintfW(d4, ARRAYSIZE(d4),
                                        L"CorpNet: Stage2 - DC address '%s' is not IPv4, skipping route check",
                                        pwszDcAddr);
                                    LOG_WARN(d4);
                                    bDcViaLan = false;  // Safe default: don't assume corporate
                                }
                            }

                            if (bDcViaLan)
                            {
                                WCHAR d3[320] = {};
                                StringCchPrintfW(d3, ARRAYSIZE(d3),
                                    L"CorpNet: Stage2 - DC '%s' reachable via LAN (not WireGuard): corporate network confirmed",
                                    pDcInfo->DomainControllerName ? pDcInfo->DomainControllerName : L"?");
                                LOG_DEBUG(d3);
                                bCorporate = true;
                            }
                            else
                            {
                                WCHAR d3[320] = {};
                                StringCchPrintfW(d3, ARRAYSIZE(d3),
                                    L"CorpNet: Stage2 - DC '%s' found but route goes via WireGuard (VPN) - not corporate LAN",
                                    pDcInfo->DomainControllerName ? pDcInfo->DomainControllerName : L"?");
                                LOG_DEBUG(d3);
                            }
                            pfnFree(pDcInfo);
                        }
                        else
                        {
                            WCHAR d3[128] = {};
                            StringCchPrintfW(d3, ARRAYSIZE(d3),
                                L"CorpNet: Stage2 - DsGetDcName err=%lu, DC not reachable", dwErr);
                            LOG_DEBUG(d3);
                        }
                    }
                    FreeLibrary(hNetApi);
                }
                else
                    LOG_WARN(L"CorpNet: Stage2 - Netapi32.dll not available");
            }
        }
    }

    pNLM->Release();
    if (bNeedCoUninit) CoUninitialize();

    WCHAR dResult[64] = {};
    StringCchPrintfW(dResult, ARRAYSIZE(dResult),
        L"CorpNet: Result = %s", bCorporate ? L"TRUE (corporate)" : L"FALSE (not corporate)");
    LOG_DEBUG(dResult);
    return bCorporate;
}

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
        // Service not found = tunnel not running; this is the normal disconnected state
        CloseServiceHandle(hSCM);
        return false;
    }
    SERVICE_STATUS_PROCESS ssp = {}; DWORD dw = 0;
    QueryServiceStatusEx(hSvc, SC_STATUS_PROCESS_INFO,
                         reinterpret_cast<LPBYTE>(&ssp), sizeof(ssp), &dw);
    CloseServiceHandle(hSvc); CloseServiceHandle(hSCM);
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

    WCHAR d[MAX_PATH_WGCP + 64] = {};
    StringCchPrintfW(d, ARRAYSIZE(d), L"WGConnect: starting tunnel '%s'", pwszProfile);
    LOG_DEBUG(d);

    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, wszCmd, nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        WCHAR e[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(e, ARRAYSIZE(e),
            L"WGConnect: CreateProcess failed for '%s' err=%lu", pwszProfile, GetLastError());
        LOG_CRIT(e); return false;
    }
    WaitForSingleObject(pi.hProcess, 10000);
    DWORD dwExit = 0;
    GetExitCodeProcess(pi.hProcess, &dwExit);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);

    if (dwExit == 0)
    {
        WCHAR r[MAX_PATH_WGCP + 32] = {};
        StringCchPrintfW(r, ARRAYSIZE(r), L"WGConnect: tunnel '%s' started successfully", pwszProfile);
        LOG_DEBUG(r);
    }
    else
    {
        WCHAR r[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(r, ARRAYSIZE(r),
            L"WGConnect: wireguard.exe exited with code=%lu for tunnel '%s'", dwExit, pwszProfile);
        LOG_WARN(r);
    }
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

    WCHAR d[MAX_PATH_WGCP + 64] = {};
    StringCchPrintfW(d, ARRAYSIZE(d), L"WGDisconnect: stopping tunnel '%s'", pwszProfile);
    LOG_DEBUG(d);

    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, wszCmd, nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        WCHAR e[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(e, ARRAYSIZE(e),
            L"WGDisconnect: CreateProcess failed for '%s' err=%lu", pwszProfile, GetLastError());
        LOG_CRIT(e);
        return false;
    }
    WaitForSingleObject(pi.hProcess, 5000);
    DWORD dwExit = 0;
    GetExitCodeProcess(pi.hProcess, &dwExit);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);

    if (dwExit != 0)
    {
        WCHAR r[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(r, ARRAYSIZE(r),
            L"WGDisconnect: wireguard.exe exited with code=%lu for tunnel '%s'", dwExit, pwszProfile);
        LOG_WARN(r);
    }
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
    StringCchPrintfW(wszTmpFile, MAX_PATH,
        L"%swgcp_stats_%lu_%lu.txt", wszTmp, GetCurrentProcessId(), GetTickCount());

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
        if      (b >= 1024LL*1024*1024) StringCchPrintfW(out, cch, L"%.1fG", b/1073741824.0);
        else if (b >= 1024*1024)        StringCchPrintfW(out, cch, L"%.1fM", b/1048576.0);
        else if (b >= 1024)             StringCchPrintfW(out, cch, L"%.1fK", b/1024.0);
        else                            StringCchPrintfW(out, cch, L"%lldB",  b);
    };
    WCHAR wszTx[16] = {}, wszRx[16] = {};
    fmtBytes(tx, wszTx, 16); fmtBytes(rx, wszRx, 16);
    // Compact format to fit Windows tooltip 128-char limit
    StringCchPrintfW(pwszOut, cchOut, L"\u2191%s  \u2193%s", wszTx, wszRx);
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
// WGGetLastHandshakeSec
// Returns seconds since the most recent WireGuard handshake across ALL peers
// for the given profile. This is deliberately the MINIMUM age (most recent),
// because the timeout should fire only when NO peer has communicated.
//
// Returns -1 if:
//   - The tunnel is not running (wg.exe not found, or service stopped)
//   - wg.exe times out or produces no output
//   - No handshake has occurred yet for any peer (timestamp == 0)
//
// Bug fixes vs. previous version:
//   1. Tmp file name is PID-unique → no collision between CP DLL and Tray
//   2. stderr is redirected to NUL, not to the parent's STD_ERROR_HANDLE
//      (which is NULL in a Windows service / LogonUI context and causes
//       CreateProcess to fail with ERROR_INVALID_HANDLE on some machines)
//   3. All peers are parsed, not just the first tab encountered
//   4. Timeout raised from 3 s to 8 s (wg.exe can be slow on loaded systems)
//   5. Return -1 (not 0) to distinguish "no handshake" from "wg.exe failed"
// ---------------------------------------------------------------------------
inline LONGLONG WGGetLastHandshakeSec(PCWSTR pwszWgExe, PCWSTR pwszProfile)
{
    if (!pwszWgExe || !pwszWgExe[0] || !pwszProfile || !pwszProfile[0])
        return -1;

    // Verify wg.exe exists before attempting to spawn it
    if (GetFileAttributesW(pwszWgExe) == INVALID_FILE_ATTRIBUTES)
    {
        WCHAR e[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(e, ARRAYSIZE(e),
            L"Handshake: wg.exe not found at '%s'", pwszWgExe);
        LOG_WARN(e);
        return -1;
    }

    // PID + Tick-Count: eindeutig auch wenn dieselbe PID die Funktion reentrant aufruft
    // (z.B. Tooltip-Timer und NetWatch-Thread rufen gleichzeitig WGGetLastHandshakeSec auf)
    WCHAR wszTmp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, wszTmp);
    WCHAR wszTmpFile[MAX_PATH] = {};
    StringCchPrintfW(wszTmpFile, MAX_PATH,
        L"%swgcp_hs_%lu_%lu.txt", wszTmp, GetCurrentProcessId(), GetTickCount());

    WCHAR wszCmd[MAX_PATH_WGCP * 2] = {};
    StringCchPrintfW(wszCmd, ARRAYSIZE(wszCmd),
        L"\"%s\" show \"%s\" latest-handshakes", pwszWgExe, pwszProfile);

    // FILE_SHARE_READ erlaubt parallele Lesezugriffe auf dieselbe Datei,
    // verhindert ERROR_SHARING_VIOLATION (err=32) wenn ein anderer Thread
    // die Datei zeitgleich liest bevor wg.exe sie vollständig beschrieben hat.
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };

    HANDLE hStdout = CreateFileW(wszTmpFile, GENERIC_WRITE, FILE_SHARE_READ, &sa,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hStdout == INVALID_HANDLE_VALUE)
    {
        WCHAR e[MAX_PATH + 64] = {};
        StringCchPrintfW(e, ARRAYSIZE(e),
            L"Handshake: cannot create temp file '%s' err=%lu", wszTmpFile, GetLastError());
        LOG_WARN(e);
        return -1;
    }

    HANDLE hStderr = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hStderr == INVALID_HANDLE_VALUE)
        hStderr = hStdout;

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdout;
    si.hStdError  = hStderr;
    si.hStdInput  = nullptr;  // no stdin needed

    PROCESS_INFORMATION pi = {};
    BOOL bOk = CreateProcessW(nullptr, wszCmd, nullptr, nullptr,
                               TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

    CloseHandle(hStdout);
    if (hStderr != hStdout) CloseHandle(hStderr);

    if (!bOk)
    {
        DeleteFileW(wszTmpFile);
        WCHAR e[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(e, ARRAYSIZE(e),
            L"Handshake: CreateProcess(wg.exe) failed err=%lu", GetLastError());
        LOG_WARN(e);
        return -1;
    }

    // Bug fix 4: Timeout raised to 8 s – wg.exe can be slow on systems
    // under load or when the WireGuard service is slow to respond.
    DWORD dwWait = WaitForSingleObject(pi.hProcess, 8000);
    DWORD dwExit = 0;
    GetExitCodeProcess(pi.hProcess, &dwExit);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (dwWait == WAIT_TIMEOUT)
    {
        DeleteFileW(wszTmpFile);
        WCHAR e[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(e, ARRAYSIZE(e),
            L"Handshake: wg.exe timed out for profile '%s'", pwszProfile);
        LOG_WARN(e);
        return -1;
    }

    if (dwExit != 0)
    {
        DeleteFileW(wszTmpFile);
        WCHAR e[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(e, ARRAYSIZE(e),
            L"Handshake: wg.exe exited with code=%lu for profile '%s'",
            dwExit, pwszProfile);
        LOG_WARN(e);
        return -1;
    }

    // Read output
    HANDLE hF = CreateFileW(wszTmpFile, GENERIC_READ, FILE_SHARE_READ,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hF == INVALID_HANDLE_VALUE)
    {
        DeleteFileW(wszTmpFile);
        return -1;
    }

    // Buffer for output: wg show latest-handshakes outputs one line per peer:
    // "<pubkey>\t<unix_timestamp>\n"
    // Allocate enough for up to 64 peers (each line ~100 chars max)
    char szBuf[8192] = {};
    DWORD dwRead = 0;
    ReadFile(hF, szBuf, sizeof(szBuf) - 1, &dwRead, nullptr);
    CloseHandle(hF);
    DeleteFileW(wszTmpFile);

    if (dwRead == 0)
    {
        // wg.exe produced no output = tunnel not running or no peers configured
        WCHAR d[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(d, ARRAYSIZE(d),
            L"Handshake: wg.exe produced no output for profile '%s' - tunnel may not be running",
            pwszProfile);
        LOG_DEBUG(d);
        return -1;
    }

    // Current Unix time for age calculation
    FILETIME ftNow = {};
    GetSystemTimeAsFileTime(&ftNow);
    ULARGE_INTEGER uliNow;
    uliNow.LowPart  = ftNow.dwLowDateTime;
    uliNow.HighPart = ftNow.dwHighDateTime;
    LONGLONG llNow = static_cast<LONGLONG>(uliNow.QuadPart / 10000000ULL) - 11644473600LL;

    // Bug fix 3: Parse ALL peer lines, return the MINIMUM age (most recent handshake).
    // The old code stopped at the first tab, so a multi-peer config would only
    // check peer #1, missing activity on other peers.
    LONGLONG llMinAge = LLONG_MAX;
    int      nPeers   = 0;
    int      nWithHS  = 0;

    char* pLine = szBuf;
    while (pLine && *pLine)
    {
        char* pEnd = strchr(pLine, '\n');
        if (pEnd) *pEnd = '\0';

        // Each line: "<pubkey>\t<timestamp>"
        char* pTab = strchr(pLine, '\t');
        if (pTab)
        {
            nPeers++;
            LONGLONG llTimestamp = _atoi64(pTab + 1);
            if (llTimestamp > 0)  // 0 = no handshake yet for this peer
            {
                nWithHS++;
                LONGLONG llAge = llNow - llTimestamp;
                if (llAge >= 0 && llAge < llMinAge)
                    llMinAge = llAge;
            }
        }

        pLine = pEnd ? pEnd + 1 : nullptr;
    }

    if (nPeers == 0)
    {
        LOG_DEBUG(L"Handshake: wg.exe output contained no peer entries");
        return -1;
    }

    if (nWithHS == 0)
    {
        WCHAR d[128] = {};
        StringCchPrintfW(d, ARRAYSIZE(d),
            L"Handshake: %d peer(s) found, none have completed a handshake yet"
            L" - returning large age to trigger timeout", nPeers);
        LOG_DEBUG(d);
        // Return a very large age so the handshake-timeout check fires:
        // A tunnel that has NEVER completed a handshake is broken and should
        // be disconnected after the configured timeout.
        return 86400LL;
    }

    // Log result for administrator visibility
    {
        WCHAR d[128] = {};
        StringCchPrintfW(d, ARRAYSIZE(d),
            L"Handshake: profile '%s' – %d peer(s), most recent handshake %lld s ago",
            pwszProfile, nPeers, llMinAge);
        LOG_DEBUG(d);
    }

    return llMinAge;
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
        SCARD_READERSTATEW rs = {};
        rs.szReader       = cfg.wszReaderName;
        rs.dwCurrentState = SCARD_STATE_UNAWARE;
        LONG lRet = SCardGetStatusChangeW(hCtx, 0, &rs, 1);
        SCardReleaseContext(hCtx);
        if (lRet == SCARD_S_SUCCESS && (rs.dwEventState & SCARD_STATE_PRESENT))
        {
            StringCchCopyW(pwszReaderOut, cchReader, cfg.wszReaderName);
            return true;
        }
        return false;
    }

    // Enumerate all readers
    DWORD dwLen = SCARD_AUTOALLOCATE;
    LPWSTR pwszReaders = nullptr;
    LONG lRet = SCardListReadersW(hCtx, nullptr,
                                   reinterpret_cast<LPWSTR>(&pwszReaders), &dwLen);
    if (lRet != SCARD_S_SUCCESS || !pwszReaders)
    {
        // SCARD_E_NO_READERS_AVAILABLE is the normal state when no reader is attached
        if (lRet != (LONG)SCARD_E_NO_READERS_AVAILABLE)
        {
            WCHAR e[64] = {};
            StringCchPrintfW(e, 64, L"SC: SCardListReaders failed err=0x%08X", lRet);
            LOG_WARN(e);
        }
        SCardReleaseContext(hCtx);
        return false;
    }

    bool bFound = false;
    for (LPCWSTR p = pwszReaders; *p; p += wcslen(p) + 1)
    {
        // Skip virtual SIM/UICC readers – they are not PIV-capable
        if (wcsstr(p, L"UICC") || wcsstr(p, L"SIM") || wcsstr(p, L"Microsoft UICC"))
            continue;

        SCARD_READERSTATEW rs = {};
        rs.szReader       = p;
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

// Waits until a card is inserted (timeout in seconds, 0 = immediate)
inline bool WGCPWaitForCard(const WGCPSmartcardConfig& cfg,
                             WCHAR* pwszReaderOut, DWORD cchReader)
{
    WCHAR d[64] = {};
    StringCchPrintfW(d, 64, L"SC: Waiting for card (timeout %lu s)", cfg.dwTimeout);
    LOG_DEBUG(d);
    DWORD dwDeadline = GetTickCount() + cfg.dwTimeout * 1000;
    do {
        if (WGCPFindSmartcard(cfg, pwszReaderOut, cchReader))
        {
            WCHAR d2[320] = {};
            StringCchPrintfW(d2, ARRAYSIZE(d2), L"SC: Card found in reader '%s'", pwszReaderOut);
            LOG_DEBUG(d2);
            return true;
        }
        Sleep(500);
    } while (GetTickCount() < dwDeadline);
    LOG_WARN(L"SC: Card wait timeout - no PIV card found within configured timeout");
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
    return (rs.dwEventState & SCARD_STATE_EMPTY) != 0;
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
        else if (lGet != SCARD_S_SUCCESS)
        {
            WCHAR eG[64] = {};
            StringCchPrintfW(eG, 64, L"SC: PIV GET DATA failed lRet=0x%08X - falling back to cert store", lGet);
            LOG_DEBUG(eG);
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
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"SC: SCardEstablishContext failed err=%lu", GetLastError());
        LOG_WARN(e);
        return WGCPScResult::Error;
    }

    LONG lRet = SCardConnectW(hCtx, wszReader,
                               SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
                               &hCard, &dwProto);
    if (lRet != SCARD_S_SUCCESS)
    {
        WCHAR e[128] = {};
        StringCchPrintfW(e, ARRAYSIZE(e),
            L"SC: SCardConnect failed lRet=0x%08X reader='%s'", lRet, wszReader);
        LOG_WARN(e);
        SCardReleaseContext(hCtx);
        return WGCPScResult::Error;
    }
    {
        WCHAR d[96] = {};
        StringCchPrintfW(d, ARRAYSIZE(d), L"SC: Connected to reader '%s' proto=%s",
                         wszReader, dwProto == SCARD_PROTOCOL_T0 ? L"T=0" : L"T=1");
        LOG_DEBUG(d);
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
        if (lSel != SCARD_S_SUCCESS)
        {
            WCHAR eSel[96] = {};
            StringCchPrintfW(eSel, ARRAYSIZE(eSel),
                L"SC: SELECT PIV failed lRet=0x%08X SW=%02X%02X",
                lSel, dwSelRecv >= 2 ? selResp[dwSelRecv-2] : 0,
                      dwSelRecv >= 1 ? selResp[dwSelRecv-1] : 0);
            LOG_WARN(eSel);
        }
        LOG_DEBUG(L"SC: Sending PIV VERIFY APDU");

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
        if (resp[0] == 0x90 && resp[1] == 0x00)
        {
            LOG_DEBUG(L"SC: PIN verification successful");
        }
        else if (resp[0] == 0x69 && resp[1] == 0x83)
        {
            SCardDisconnect(hCard, SCARD_LEAVE_CARD);
            SCardReleaseContext(hCtx);
            LOG_WARN(L"SC: PIN locked - user must reset PIN with YubiKey Manager");
            return WGCPScResult::PinLocked;
        }
        else
        {
            DWORD remaining = resp[1] & 0x0F;
            WCHAR e[96] = {};
            StringCchPrintfW(e, ARRAYSIZE(e),
                L"SC: Wrong PIN (SW=%02X%02X) - %lu attempt(s) remaining",
                resp[0], resp[1], remaining);
            LOG_WARN(e);
            SCardDisconnect(hCard, SCARD_LEAVE_CARD);
            SCardReleaseContext(hCtx);
            return WGCPScResult::PinWrong;
        }
    }

    SCardDisconnect(hCard, SCARD_LEAVE_CARD);
    SCardReleaseContext(hCtx);
    LOG_DEBUG(L"SC: Authentication completed successfully");
    return WGCPScResult::Success;
}
