//
// WireGuardShutdownService.cpp
//
// Minimaler Windows Service der NUR beim PC-Shutdown (PRESHUTDOWN) alle
// aktiven WireGuard-Tunnel trennt. Er tut sonst NICHTS.
//
// Installation:  WireGuardShutdownService.exe /install
// Deinstallation: WireGuardShutdownService.exe /uninstall
// Manueller Test: WireGuardShutdownService.exe /run
//

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define STRSAFE_NO_DEPRECATE
#include <windows.h>
#include <strsafe.h>
#include <winsvc.h>

#pragma comment(lib, "advapi32.lib")

#define SVC_NAME        L"WireGuardShutdownHelper"
#define SVC_DISPLAY     L"WireGuard Shutdown Helper"
#define SVC_DESC        L"Trennt aktive WireGuard-Tunnel beim Herunterfahren des PCs."

#define REG_KEY         L"SOFTWARE\\WireGuardCredentialProvider"
#define REG_EXEPATH     L"ExePath"
#define DEFAULT_EXEPATH L"C:\\Program Files\\WireGuard\\wireguard.exe"
#define WG_CONFIG_DIR   L"C:\\Program Files\\WireGuard\\Data\\Configurations\\"
#define WG_CONFIG_EXT   L".conf.dpapi"
#define WG_SVC_PREFIX   L"WireGuardTunnel$"

#define MAX_BUF      1024
#define MAX_PROFILES   64

static SERVICE_STATUS_HANDLE g_hSvcStatus = nullptr;
static HANDLE                g_hStopEvent  = nullptr;

// ---------------------------------------------------------------------------
static void SetSvcStatus(DWORD dwState, DWORD dwAccept = 0)
{
    SERVICE_STATUS ss = {};
    ss.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
    ss.dwCurrentState     = dwState;
    ss.dwControlsAccepted = dwAccept;
    ss.dwWin32ExitCode    = NO_ERROR;
    ss.dwWaitHint         = (dwState == SERVICE_START_PENDING || 
                              dwState == SERVICE_STOP_PENDING) ? 30000 : 0;
    SetServiceStatus(g_hSvcStatus, &ss);
}

static void LogEvent(PCWSTR pwszMsg)
{
    HANDLE hLog = RegisterEventSourceW(nullptr, SVC_NAME);
    if (hLog)
    {
        ReportEventW(hLog, EVENTLOG_INFORMATION_TYPE, 0, 0,
                     nullptr, 1, 0, &pwszMsg, nullptr);
        DeregisterEventSource(hLog);
    }
}

static int EnumProfiles(WCHAR profiles[][MAX_BUF], int maxProfiles)
{
    int count = 0;
    WCHAR wszSearch[MAX_BUF] = {};
    StringCchCopyW(wszSearch, MAX_BUF, WG_CONFIG_DIR);
    StringCchCatW(wszSearch,  MAX_BUF, L"*");
    StringCchCatW(wszSearch,  MAX_BUF, WG_CONFIG_EXT);

    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(wszSearch, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;
    do {
        if (count >= maxProfiles) break;
        WCHAR wszName[MAX_BUF] = {};
        StringCchCopyW(wszName, MAX_BUF, fd.cFileName);
        size_t extLen  = wcslen(WG_CONFIG_EXT);
        size_t nameLen = wcslen(wszName);
        if (nameLen > extLen) wszName[nameLen - extLen] = L'\0';
        StringCchCopyW(profiles[count++], MAX_BUF, wszName);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return count;
}

static bool IsTunnelActive(PCWSTR pwszProfile)
{
    WCHAR wszSvc[MAX_BUF] = {};
    StringCchCopyW(wszSvc, MAX_BUF, WG_SVC_PREFIX);
    StringCchCatW(wszSvc,  MAX_BUF, pwszProfile);
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

static void GetExePath(WCHAR* pwszOut, DWORD cchOut)
{
    StringCchCopyW(pwszOut, cchOut, DEFAULT_EXEPATH);
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD dwType = REG_SZ, cbData = cchOut * sizeof(WCHAR);
        (void)RegQueryValueExW(hKey, REG_EXEPATH, nullptr, &dwType,
                               reinterpret_cast<LPBYTE>(pwszOut), &cbData);
        RegCloseKey(hKey);
    }
}

// ---------------------------------------------------------------------------
// Hauptaufgabe: alle aktiven Tunnel trennen
// Wird NUR beim PRESHUTDOWN-Event aufgerufen
// ---------------------------------------------------------------------------
static void DisconnectAllTunnels()
{
    WCHAR wszExePath[MAX_BUF] = {};
    GetExePath(wszExePath, MAX_BUF);

    WCHAR profiles[MAX_PROFILES][MAX_BUF] = {};
    int nProfiles = EnumProfiles(profiles, MAX_PROFILES);

    WCHAR wszLog[MAX_BUF] = {};
    StringCchPrintfW(wszLog, MAX_BUF, L"Shutdown: %d Profile gefunden, pruefe Verbindungen...", nProfiles);
    LogEvent(wszLog);

    int nDisconnected = 0;
    for (int i = 0; i < nProfiles; i++)
    {
        if (!IsTunnelActive(profiles[i])) continue;

        WCHAR wszCmd[MAX_BUF * 2] = {};
        StringCchPrintfW(wszCmd, ARRAYSIZE(wszCmd),
                         L"\"%s\" /uninstalltunnelservice %s",
                         wszExePath, profiles[i]);

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        if (CreateProcessW(nullptr, wszCmd, nullptr, nullptr,
                           FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 8000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            nDisconnected++;

            StringCchPrintfW(wszLog, MAX_BUF, L"Tunnel '%s' beim Shutdown getrennt.", profiles[i]);
            LogEvent(wszLog);
        }
        else
        {
            StringCchPrintfW(wszLog, MAX_BUF,
                             L"Fehler beim Trennen von '%s': %lu", profiles[i], GetLastError());
            LogEvent(wszLog);
        }
    }

    if (nDisconnected == 0)
        LogEvent(L"Shutdown: Keine aktiven WireGuard-Tunnel gefunden.");
    else
    {
        StringCchPrintfW(wszLog, MAX_BUF, L"Shutdown: %d Tunnel(s) getrennt.", nDisconnected);
        LogEvent(wszLog);
    }
}

// ---------------------------------------------------------------------------
// Service Control Handler
// ---------------------------------------------------------------------------
static DWORD WINAPI SvcCtrlHandler(DWORD dwCtrl, DWORD, LPVOID, LPVOID)
{
    switch (dwCtrl)
    {
    case SERVICE_CONTROL_STOP:
        SetSvcStatus(SERVICE_STOP_PENDING);
        SetEvent(g_hStopEvent);
        return NO_ERROR;

    case SERVICE_CONTROL_PRESHUTDOWN:
        // Das ist der Moment den wir brauchen
        LogEvent(L"PRESHUTDOWN empfangen - trenne alle WireGuard-Tunnel...");
        SetSvcStatus(SERVICE_STOP_PENDING);
        DisconnectAllTunnels();
        SetEvent(g_hStopEvent);
        return NO_ERROR;

    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;
    }
    return ERROR_CALL_NOT_IMPLEMENTED;
}

// ---------------------------------------------------------------------------
// Service Main - wartet einfach nur auf PRESHUTDOWN oder STOP
// ---------------------------------------------------------------------------
static VOID WINAPI ServiceMain(DWORD, LPWSTR*)
{
    g_hSvcStatus = RegisterServiceCtrlHandlerExW(SVC_NAME, SvcCtrlHandler, nullptr);
    if (!g_hSvcStatus) return;

    g_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_hStopEvent) { SetSvcStatus(SERVICE_STOPPED); return; }

    // PRESHUTDOWN + STOP akzeptieren, sonst nichts
    SERVICE_STATUS ss = {};
    ss.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
    ss.dwCurrentState     = SERVICE_RUNNING;
    ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PRESHUTDOWN;
    ss.dwWin32ExitCode    = NO_ERROR;
    SetServiceStatus(g_hSvcStatus, &ss);

    LogEvent(L"WireGuard Shutdown Helper laeuft - wartet auf Shutdown.");

    // Blockieren bis STOP oder PRESHUTDOWN
    WaitForSingleObject(g_hStopEvent, INFINITE);

    CloseHandle(g_hStopEvent);
    g_hStopEvent = nullptr;
    SetSvcStatus(SERVICE_STOPPED);
}

// ---------------------------------------------------------------------------
// Install / Uninstall / Run
// ---------------------------------------------------------------------------
static void Install()
{
    WCHAR wszPath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, wszPath, MAX_PATH);

    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!hSCM) { wprintf(L"OpenSCManager Fehler: %lu\n", GetLastError()); return; }

    SC_HANDLE hSvc = CreateServiceW(
        hSCM, SVC_NAME, SVC_DISPLAY,
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        wszPath, nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!hSvc)
    {
        DWORD dwErr = GetLastError();
        if (dwErr == ERROR_SERVICE_EXISTS)
        {
            wprintf(L"Service existiert bereits - aktualisiere...\n");
            hSvc = OpenServiceW(hSCM, SVC_NAME, SERVICE_CHANGE_CONFIG | SERVICE_START);
            if (hSvc) ChangeServiceConfigW(hSvc, SERVICE_NO_CHANGE, SERVICE_AUTO_START,
                                           SERVICE_NO_CHANGE, wszPath, nullptr, nullptr,
                                           nullptr, nullptr, nullptr, nullptr);
        }
        else
        {
            wprintf(L"CreateService Fehler: %lu\n", dwErr);
            CloseServiceHandle(hSCM); return;
        }
    }

    if (hSvc)
    {
        SERVICE_DESCRIPTIONW desc = { const_cast<LPWSTR>(SVC_DESC) };
        ChangeServiceConfig2W(hSvc, SERVICE_CONFIG_DESCRIPTION, &desc);

        // 30 Sekunden fuer Tunnel-Trennung beim Shutdown
        SERVICE_PRESHUTDOWN_INFO psi = { 30000 };
        ChangeServiceConfig2W(hSvc, SERVICE_CONFIG_PRESHUTDOWN_INFO, &psi);

        wprintf(L"Service installiert. Starte...\n");
        StartServiceW(hSvc, 0, nullptr);
        wprintf(L"Fertig. Service '%s' laeuft.\n", SVC_NAME);
        CloseServiceHandle(hSvc);
    }
    CloseServiceHandle(hSCM);
}

static void Uninstall()
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) { wprintf(L"OpenSCManager Fehler: %lu\n", GetLastError()); return; }

    SC_HANDLE hSvc = OpenServiceW(hSCM, SVC_NAME,
                                  SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
    if (!hSvc) { wprintf(L"Service nicht gefunden.\n"); CloseServiceHandle(hSCM); return; }

    SERVICE_STATUS ss = {};
    ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
    Sleep(1500);

    DeleteService(hSvc) ? wprintf(L"Service entfernt.\n")
                        : wprintf(L"Fehler: %lu\n", GetLastError());
    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc >= 2)
    {
        if (_wcsicmp(argv[1], L"/install") == 0)   { Install();   return 0; }
        if (_wcsicmp(argv[1], L"/uninstall") == 0) { Uninstall(); return 0; }
        if (_wcsicmp(argv[1], L"/run") == 0)
        {
            wprintf(L"Manueller Test: Trenne alle aktiven WireGuard-Tunnel...\n");
            DisconnectAllTunnels();
            wprintf(L"Fertig.\n");
            return 0;
        }
        wprintf(L"Verwendung: %s [/install | /uninstall | /run]\n", argv[0]);
        return 1;
    }

    static SERVICE_TABLE_ENTRYW svcTable[] =
    {
        { const_cast<LPWSTR>(SVC_NAME), ServiceMain },
        { nullptr, nullptr }
    };
    StartServiceCtrlDispatcherW(svcTable);
    return 0;
}
