#pragma once
//
// WireGuardTray.h
//
// Post-logon Tray Application for WireGuard Credential Provider.
// Replaces the WireGuard-UI for domain-joined clients.
//
// - Reads configuration from the SAME registry key as the Credential Provider:
//     HKLM\SOFTWARE\Jens Kaesler\WireGuard Credential Provider
// - SmartcardEnabled = 1  -->  PIN dialog before every connect
// - SmartcardEnabled = 0  -->  direct connect (stock WireGuard UI behavior)
// - Autostart via HKLM Run key (set by installer, applies to all users)
// - Original wireguard.exe autostart is removed by the installer
//

// ---------------------------------------------------------------------------
// FIX: WIN32 macros BEFORE any includes – helpers.h also defines these,
//      so guard against redefinition warnings with #ifndef
// ---------------------------------------------------------------------------
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef STRSAFE_NO_DEPRECATE          // helpers.h defines this too → guard it
#define STRSAFE_NO_DEPRECATE
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN7
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <strsafe.h>
#include <winscard.h>
#include <wincrypt.h>
#include <new>          // std::nothrow
#include <tlhelp32.h>   // CreateToolhelp32Snapshot, PROCESSENTRY32W

// ---------------------------------------------------------------------------
// Pull in all shared helpers from the CP DLL.
// WGCP_TRAY_BUILD strips the credentialprovider.h-dependent block.
// ---------------------------------------------------------------------------
#define WGCP_TRAY_BUILD
#include "../../src/helpers.h"
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define WM_TRAYICON         (WM_USER + 1)
// Resource IDs are defined in resources/resource.h (pulled in via helpers.h)
// IDI_TRAY_CONNECTED (103) and IDI_TRAY_DISCONNECTED (104) are also in WireGuardTray.rc

#define TIMER_REFRESH_ID    1
#define TIMER_REFRESH_MS    5000    // 5 s auto-refresh (matches CP tile)

// Context menu command IDs
#define IDM_CONNECT         200
#define IDM_DISCONNECT      201
#define IDM_PROFILE_BASE    300     // 300..363 for up to MAX_PROFILES profiles
#define IDM_IMPORT          401
#define IDM_OPEN_CONFIG_DIR 402
#define IDM_DELETE_PROFILE  403
#define IDM_OPEN_YKMANAGER  404
#define IDM_EXIT            400

#define WGCP_TRAY_CLASS     L"WireGuardCPTrayClass"
#define WGCP_TRAY_MUTEX     L"WireGuardCPTray_SingleInstance"

// PIN dialog control IDs (programmatic dialog – no .rc dialog template needed)
#define IDC_PIN_EDIT        502
#define IDC_SC_STATUS       505

// ---------------------------------------------------------------------------
// WireGuardTrayApp
// ---------------------------------------------------------------------------
class WireGuardTrayApp
{
public:
    WireGuardTrayApp();
    ~WireGuardTrayApp();

    bool Init(HINSTANCE hInst);
    int  Run();

private:
    HINSTANCE           _hInst;
    HWND                _hWnd;
    NOTIFYICONDATAW     _nid;
    HICON               _hIconConnected;
    HICON               _hIconDisconnected;

    bool                _bConnected;
    WCHAR               _wszCurrentProfile[MAX_PATH_WGCP];

    WCHAR               _wszExePath[MAX_PATH_WGCP];
    WCHAR               _wszWgExePath[MAX_PATH_WGCP];
    DWORD               _dwHandshakeTimeoutSec; // 0 = disabled
    WGCPSmartcardConfig _scConfig;

    WCHAR               _rgProfiles[MAX_PROFILES][MAX_PATH_WGCP];
    int                 _nProfiles;
    int                 _nSelectedProfile;

    WCHAR               _wszPin[64];
    WCHAR               _wszScStatusMsg[MAX_LABEL_WGCP];

    static LRESULT CALLBACK _WndProc(HWND, UINT, WPARAM, LPARAM);
    static INT_PTR CALLBACK _PinDlgProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT _HandleMessage(HWND, UINT, WPARAM, LPARAM);

    void _LoadConfig();
    void _LoadProfiles();
    void _RefreshStatus();
    void _UpdateTrayIcon();
    void _ShowContextMenu();
    void _Connect(int profileIndex);
    void _Disconnect();
    bool _DoSmartcardAuth();
    bool _ShowPinDialog();
    void _AddTrayIcon();
    void _RemoveTrayIcon();
    void _UpdateTrayTooltip();
    void _ImportProfile();
    void _DeleteProfile();
    void _OpenYubiKeyManager();
    WCHAR _wszYkMgrPath[MAX_PATH];  // path found during menu build
    void _OpenConfigDir();
    void _CheckAndRemoveWireGuardShortcut();

    // WireGuard UI Watcher
    void _DisableWireGuardManager();
    void _StartWireGuardWatcher();
    void _StopWireGuardWatcher();
    static DWORD WINAPI _WatcherThread(LPVOID lpParam);
    HANDLE _hWatcherThread;
    HANDLE _hWatcherStop;

    // Smartcard presence watcher (auto-connect / auto-disconnect)
    void _StartSmartcardWatcher();
    void _StopSmartcardWatcher();
    static DWORD WINAPI _SmartcardWatchThread(LPVOID lpParam);
    HANDLE _hScWatchThread;
    HANDLE _hScWatchStop;

    // Corporate network watcher (auto-disconnect when on corp network)
    void _StartNetworkWatcher();
    void _StopNetworkWatcher();
    static DWORD WINAPI _NetworkWatchThread(LPVOID lpParam);
    HANDLE _hNetWatchThread;
    HANDLE _hNetWatchStop;

    void _Log(DWORD level, PCWSTR msg)
    {
        WCHAR buf[1024] = {};
        StringCchPrintfW(buf, ARRAYSIZE(buf), L"[Tray] %s", msg);
        WGCPLog(level, buf);
    }
};
