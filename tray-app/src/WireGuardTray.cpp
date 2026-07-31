//
// WireGuardTray.cpp
//
// Post-logon WireGuard Tray Application.
// Part of the WireGuard Credential Provider project.
//
// Encoding: UTF-8 with BOM
// All user-visible strings are bilingual (DE/EN) based on system locale.
//

#include "WireGuardTray.h"
#include <new>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

static WireGuardTrayApp* g_pApp = nullptr;

// ---------------------------------------------------------------------------
// Language detection – returns true if system UI is German
// ---------------------------------------------------------------------------
static bool IsSystemLanguageGerman()
{
    // Alle verfuegbaren Sprach-APIs pruefen
    LANGID ids[] = {
        GetUserDefaultUILanguage(),
        GetSystemDefaultUILanguage(),
        GetUserDefaultLangID(),
        LANGIDFROMLCID(GetThreadLocale()),
        LANGIDFROMLCID(GetUserDefaultLCID()),
    };
    for (LANGID lid : ids)
        if (PRIMARYLANGID(lid) == LANG_GERMAN) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Bilingual string helper
// ---------------------------------------------------------------------------
static PCWSTR T(PCWSTR de, PCWSTR en)
{
    return IsSystemLanguageGerman() ? de : en;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
WireGuardTrayApp::WireGuardTrayApp()
    : _hInst(nullptr), _hWnd(nullptr)
    , _hIconConnected(nullptr), _hIconDisconnected(nullptr)
    , _bConnected(false)
    , _nProfiles(0), _nSelectedProfile(0)
{
    _hWatcherThread  = nullptr;
    _hWatcherStop    = nullptr;
    _hScWatchThread  = nullptr;
    _hScWatchStop    = nullptr;
    ZeroMemory(&_nid,              sizeof(_nid));
    ZeroMemory(&_scConfig,         sizeof(_scConfig));
    ZeroMemory(_wszExePath,        sizeof(_wszExePath));
    ZeroMemory(_wszWgExePath,      sizeof(_wszWgExePath));
    ZeroMemory(_wszCurrentProfile, sizeof(_wszCurrentProfile));
    ZeroMemory(_rgProfiles,        sizeof(_rgProfiles));
    ZeroMemory(_wszPin,            sizeof(_wszPin));
    ZeroMemory(_wszScStatusMsg,    sizeof(_wszScStatusMsg));
}

WireGuardTrayApp::~WireGuardTrayApp()
{
    _StopWireGuardWatcher();
    _StopSmartcardWatcher();
    _RemoveTrayIcon();
    if (_hIconConnected)    { DestroyIcon(_hIconConnected);    _hIconConnected    = nullptr; }
    if (_hIconDisconnected) { DestroyIcon(_hIconDisconnected); _hIconDisconnected = nullptr; }
    SecureZeroMemory(_wszPin, sizeof(_wszPin));
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
bool WireGuardTrayApp::Init(HINSTANCE hInst)
{
    _hInst = hInst;
    g_pApp = this;

    LOG_DEBUG(L"=== WireGuardTray Init ===");

    _LoadConfig();
    _LoadProfiles();
    WGCPLoadSmartcardConfig(_scConfig);

    // ICO-Ressourcen direkt laden (unterstuetzt Alpha und mehrere Groessen)
    _hIconConnected = static_cast<HICON>(
        LoadImageW(hInst, MAKEINTRESOURCEW(IDI_TRAY_CONNECTED),
                   IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    if (!_hIconConnected)
        _hIconConnected = LoadIconW(nullptr, IDI_APPLICATION);

    _hIconDisconnected = static_cast<HICON>(
        LoadImageW(hInst, MAKEINTRESOURCEW(IDI_TRAY_DISCONNECTED),
                   IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    if (!_hIconDisconnected)
        _hIconDisconnected = LoadIconW(nullptr, IDI_APPLICATION);

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = _WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = WGCP_TRAY_CLASS;
    if (!RegisterClassExW(&wc))
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"RegisterClassEx failed: %lu", GetLastError());
        LOG_CRIT(e);
        MessageBoxW(nullptr, e, L"WireGuard CP Tray", MB_ICONERROR | MB_OK);
        return false;
    }

    _hWnd = CreateWindowExW(0, WGCP_TRAY_CLASS, L"WireGuard CP Tray",
                             0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hInst, nullptr);
    if (!_hWnd)
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"CreateWindow failed: %lu", GetLastError());
        LOG_CRIT(e);
        MessageBoxW(nullptr, e, L"WireGuard CP Tray", MB_ICONERROR | MB_OK);
        return false;
    }

    _RefreshStatus();
    _AddTrayIcon();
    SetTimer(_hWnd, TIMER_REFRESH_ID, TIMER_REFRESH_MS, nullptr);

    _StartWireGuardWatcher();
    if (_scConfig.bEnabled &&
        (_scConfig.bConnectOnInsert || _scConfig.bDisconnectOnRemove))
        _StartSmartcardWatcher();
    LOG_DEBUG(L"Tray: Init complete");
    return true;
}

// ---------------------------------------------------------------------------
// Run
// ---------------------------------------------------------------------------
int WireGuardTrayApp::Run()
{
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

// ---------------------------------------------------------------------------
// _LoadConfig
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_LoadConfig()
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, WGCP_REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        ReadRegString(hKey, WGCP_REG_EXEPATH,   _wszExePath,   MAX_PATH_WGCP, WGCP_DEFAULT_EXEPATH);
        ReadRegString(hKey, WGCP_REG_WGEXEPATH, _wszWgExePath, MAX_PATH_WGCP, WGCP_DEFAULT_WGEXEPATH);
        RegCloseKey(hKey);
        LOG_DEBUG(L"Tray: Config loaded from registry");
    }
    else
    {
        LOG_WARN(L"Tray: Registry key not found, using compiled-in defaults");
        StringCchCopyW(_wszExePath,   MAX_PATH_WGCP, WGCP_DEFAULT_EXEPATH);
        StringCchCopyW(_wszWgExePath, MAX_PATH_WGCP, WGCP_DEFAULT_WGEXEPATH);
    }
}

// ---------------------------------------------------------------------------
// _LoadProfiles
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_LoadProfiles()
{
    _nProfiles = WGEnumProfiles(_rgProfiles, MAX_PROFILES);
    _nSelectedProfile = 0;

    WCHAR d[128] = {};
    StringCchPrintfW(d, 128, L"Tray: %d profile(s) found", _nProfiles);
    LOG_DEBUG(d);

    if (_nProfiles == 0) return;

    WCHAR wszComp[MAX_PATH_WGCP] = {};
    DWORD dwSize = MAX_PATH_WGCP;
    GetComputerNameW(wszComp, &dwSize);

    for (int i = 0; i < _nProfiles; i++)
    {
        if (_wcsicmp(_rgProfiles[i], wszComp) == 0)
        {
            _nSelectedProfile = i;
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// _RefreshStatus
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_RefreshStatus()
{
    if (_nProfiles == 0 || _nSelectedProfile >= _nProfiles)
    {
        _bConnected = false;
        return;
    }
    PCWSTR pwszProfile = _rgProfiles[_nSelectedProfile];
    _bConnected = WGIsTunnelConnected(pwszProfile);
    StringCchCopyW(_wszCurrentProfile, MAX_PATH_WGCP, pwszProfile);
}

// ---------------------------------------------------------------------------
// Tray icon
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_AddTrayIcon()
{
    ZeroMemory(&_nid, sizeof(_nid));
    _nid.cbSize           = sizeof(_nid);
    _nid.hWnd             = _hWnd;
    _nid.uID              = 1;
    _nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    _nid.uCallbackMessage = WM_TRAYICON;
    _nid.hIcon            = _bConnected ? _hIconConnected : _hIconDisconnected;
    _UpdateTrayTooltip();

    // Retry loop: taskbar may not yet accept icons on autostart (race condition)
    for (int i = 0; i < 10; i++)
    {
        if (Shell_NotifyIconW(NIM_ADD, &_nid))
        {
            LOG_DEBUG(L"Tray: Icon added successfully");
            return;
        }
        LOG_WARN(L"Tray: Shell_NotifyIconW NIM_ADD failed, retrying...");
        Sleep(1000);
    }
    LOG_CRIT(L"Tray: Shell_NotifyIconW NIM_ADD failed after 10 retries");
}

void WireGuardTrayApp::_RemoveTrayIcon()
{
    if (_nid.hWnd) Shell_NotifyIconW(NIM_DELETE, &_nid);
}

void WireGuardTrayApp::_UpdateTrayIcon()
{
    _nid.uFlags = NIF_ICON | NIF_TIP;
    _nid.hIcon  = _bConnected ? _hIconConnected : _hIconDisconnected;
    _UpdateTrayTooltip();
    Shell_NotifyIconW(NIM_MODIFY, &_nid);
}

void WireGuardTrayApp::_UpdateTrayTooltip()
{
    WCHAR wszTraffic[MAX_LABEL_WGCP] = {};
    if (_bConnected && _nProfiles > 0)
        WGGetTrafficStats(_wszWgExePath, _rgProfiles[_nSelectedProfile], wszTraffic, MAX_LABEL_WGCP);

    PCWSTR pwszProfile = (_nProfiles > 0) ? _rgProfiles[_nSelectedProfile] : L"";

    if (_bConnected)
    {
        WCHAR wszTimer[MAX_LABEL_WGCP] = {};
        if (_nProfiles > 0)
            WGGetConnectedSince(_rgProfiles[_nSelectedProfile], wszTimer, MAX_LABEL_WGCP);

        if (wszTraffic[0])
            StringCchPrintfW(_nid.szTip, ARRAYSIZE(_nid.szTip),
                             L"WireGuard VPN\n\u25CF %s\n%s\n%s",
                             pwszProfile,
                             wszTimer[0] ? wszTimer : L"",
                             wszTraffic);
        else
            StringCchPrintfW(_nid.szTip, ARRAYSIZE(_nid.szTip),
                             L"WireGuard VPN\n\u25CF %s\n%s",
                             pwszProfile,
                             T(L"Verbunden", L"Connected"));
    }
    else
    {
        if (pwszProfile[0])
            StringCchPrintfW(_nid.szTip, ARRAYSIZE(_nid.szTip),
                             L"WireGuard VPN\n\u25CB %s\n%s",
                             pwszProfile,
                             T(L"Getrennt", L"Disconnected"));
        else
            StringCchPrintfW(_nid.szTip, ARRAYSIZE(_nid.szTip),
                             L"WireGuard VPN\n\u25CB %s",
                             T(L"Getrennt", L"Disconnected"));
    }
}

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_ShowContextMenu()
{
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // Line 1: connection status
    WCHAR wszHeader[MAX_PATH_WGCP + 32] = {};
    if (_bConnected)
        StringCchPrintfW(wszHeader, ARRAYSIZE(wszHeader),
                         L"\u25CF %s", T(L"Verbunden", L"Connected"));
    else
        StringCchPrintfW(wszHeader, ARRAYSIZE(wszHeader),
                         L"\u25CB %s", T(L"Getrennt", L"Disconnected"));
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, wszHeader);

    // Line 2: active profile (shown whenever at least one profile exists)
    if (_nProfiles > 0)
    {
        WCHAR wszProfLine[MAX_PATH_WGCP + 16] = {};
        StringCchPrintfW(wszProfLine, ARRAYSIZE(wszProfLine),
                         L"%s: %s",
                         T(L"Profil", L"Profile"),
                         _rgProfiles[_nSelectedProfile]);
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, wszProfLine);
    }
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    if (_nProfiles > 1)
    {
        HMENU hSub = CreatePopupMenu();
        if (hSub)
        {
            for (int i = 0; i < _nProfiles; i++)
            {
                UINT uFlags = MF_STRING;
                if (i == _nSelectedProfile) uFlags |= MF_CHECKED;
                AppendMenuW(hSub, uFlags,
                            static_cast<UINT_PTR>(IDM_PROFILE_BASE + i),
                            _rgProfiles[i]);
            }
            AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSub),
                        T(L"Profil", L"Profile"));
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        }
    }

    if (_nProfiles > 0)
    {
        if (_bConnected)
            AppendMenuW(hMenu, MF_STRING, IDM_DISCONNECT,
                        T(L"\u23CF  Trennen", L"\u23CF  Disconnect"));
        else
            AppendMenuW(hMenu, MF_STRING, IDM_CONNECT,
                        T(L"\u25B6  Verbinden", L"\u25B6  Connect"));
    }
    else
    {
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0,
                    T(L"Kein Profil gefunden", L"No profiles found"));
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_IMPORT,
                T(L"\U0001F4C2  Profil importieren (.conf)...",
                  L"\U0001F4C2  Import profile (.conf)..."));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT,
                T(L"Beenden", L"Exit"));

    SetForegroundWindow(_hWnd);
    POINT pt = {};
    GetCursorPos(&pt);

    // Apply dark theme to the popup menu if Windows is in dark mode
    HKEY hThemeKey = nullptr;
    DWORD dwLight = 1, dwSz = sizeof(dwLight);
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hThemeKey) == ERROR_SUCCESS)
    {
        RegQueryValueExW(hThemeKey, L"AppsUseLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&dwLight), &dwSz);
        RegCloseKey(hThemeKey);
    }
    if (dwLight == 0) // Dark Mode aktiv
    {
        HMODULE hUx = LoadLibraryW(L"uxtheme.dll");
        if (hUx)
        {
            typedef HRESULT(WINAPI* fnSetWindowTheme)(HWND, LPCWSTR, LPCWSTR);
            auto pfn = reinterpret_cast<fnSetWindowTheme>(
                GetProcAddress(hUx, "SetWindowTheme"));
            if (pfn) pfn(_hWnd, L"DarkMode_Explorer", nullptr);
            FreeLibrary(hUx);
        }
    }

    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                   pt.x, pt.y, 0, _hWnd, nullptr);
    PostMessageW(_hWnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

// ---------------------------------------------------------------------------
// _Connect
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_Connect(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= _nProfiles) return;

    _nSelectedProfile = profileIndex;
    PCWSTR pwszProfile = _rgProfiles[_nSelectedProfile];

    if (_scConfig.bEnabled)
    {
        LOG_DEBUG(L"Tray: Smartcard auth required before connect");
        if (!_DoSmartcardAuth())
        {
            LOG_WARN(L"Tray: Smartcard auth failed - connect aborted");
            return;
        }
    }

    LOG_DEBUG(L"Tray: Rufe WGConnect auf...");
    bool bConnectCalled = WGConnect(_wszExePath, pwszProfile);
    WCHAR wszConnLog[64] = {};
    StringCchPrintfW(wszConnLog, 64, L"Tray: WGConnect returned %d", bConnectCalled);
    LOG_DEBUG(wszConnLog);

    bool bOk = false;
    for (int i = 0; i < 12; i++)
    {
        Sleep(500);
        bool bRunning = WGIsTunnelConnected(pwszProfile);
        WCHAR wszPoll[64] = {};
        StringCchPrintfW(wszPoll, 64, L"Tray: Poll %d/12 running=%d", i+1, bRunning);
        LOG_DEBUG(wszPoll);
        if (bRunning) { bOk = true; break; }
    }

    _RefreshStatus();
    _UpdateTrayIcon();

    // Keine eigene Notification - WireGuard zeigt selbst eine Meldung
    if (!bOk)
    {
        MessageBoxW(_hWnd,
                    T(L"Tunnel konnte nicht gestartet werden.",
                      L"Tunnel could not be started."),
                    L"WireGuard VPN", MB_ICONWARNING | MB_OK);
    }
}

// ---------------------------------------------------------------------------
// _Disconnect
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_Disconnect()
{
    if (_nProfiles == 0) return;
    PCWSTR pwszProfile = _rgProfiles[_nSelectedProfile];
    WGDisconnect(_wszExePath, pwszProfile);
    for (int i = 0; i < 12; i++)
    {
        Sleep(500);
        if (!WGIsTunnelConnected(pwszProfile)) break;
    }
    _RefreshStatus();
    _UpdateTrayIcon();
}

// ---------------------------------------------------------------------------
// _DoSmartcardAuth
// ---------------------------------------------------------------------------
bool WireGuardTrayApp::_DoSmartcardAuth()
{
    if (!_scConfig.bEnabled) return true;

    if (_scConfig.bPinRequired)
    {
        SecureZeroMemory(_wszPin, sizeof(_wszPin));
        if (!_ShowPinDialog())
        {
            LOG_DEBUG(L"Tray: SC: PIN dialog cancelled");
            return false;
        }
    }

    WGCPScResult result = WGCPAuthenticateSmartcard(_scConfig, _wszPin);
    SecureZeroMemory(_wszPin, sizeof(_wszPin));

    WCHAR wszMsg[256] = {};
    bool bOk = false;

    switch (result)
    {
    case WGCPScResult::Success:
        bOk = true;
        break;
    case WGCPScResult::Timeout:
    case WGCPScResult::NoCard:
        StringCchCopyW(wszMsg, ARRAYSIZE(wszMsg),
                       T(L"Keine Smartcard / YubiKey gefunden.\nBitte Karte einstecken und erneut versuchen.",
                         L"No smartcard / YubiKey found.\nPlease insert your card and try again."));
        break;
    case WGCPScResult::WrongCard:
        StringCchCopyW(wszMsg, ARRAYSIZE(wszMsg),
                       T(L"Falscher YubiKey.\nZertifikat-Fingerabdruck stimmt nicht \u00fcberein.",
                         L"Wrong YubiKey.\nCertificate thumbprint does not match."));
        break;
    case WGCPScResult::PinWrong:
        StringCchCopyW(wszMsg, ARRAYSIZE(wszMsg),
                       T(L"Falscher PIN.\nBitte PIN pr\u00fcfen und erneut versuchen.",
                         L"Wrong PIN.\nPlease check your PIN and try again."));
        break;
    case WGCPScResult::PinLocked:
        StringCchCopyW(wszMsg, ARRAYSIZE(wszMsg),
                       T(L"PIN gesperrt.\nBitte PIN mit YubiKey Manager entsperren.",
                         L"PIN locked.\nPlease unlock your YubiKey with YubiKey Manager."));
        break;
    case WGCPScResult::Disabled:
        bOk = true;
        break;
    default:
        StringCchCopyW(wszMsg, ARRAYSIZE(wszMsg),
                       T(L"Smartcard-Fehler.\nBitte erneut versuchen.",
                         L"Smartcard error.\nPlease try again."));
        break;
    }

    if (!bOk && wszMsg[0])
        MessageBoxW(_hWnd, wszMsg,
                    T(L"WireGuard VPN \u2013 YubiKey-Authentifizierung",
                      L"WireGuard VPN \u2013 YubiKey Authentication"),
                    MB_ICONWARNING | MB_OK);

    return bOk;
}

// ---------------------------------------------------------------------------
// _ShowPinDialog
// ---------------------------------------------------------------------------
bool WireGuardTrayApp::_ShowPinDialog()
{
    SecureZeroMemory(_wszPin, sizeof(_wszPin));

    struct PinDlgData { WireGuardTrayApp* pApp; bool bOk; };
    PinDlgData data = { this, false };

    const DWORD tmplSize = sizeof(DLGTEMPLATE) + 4;
    LPCDLGTEMPLATEW pTmpl = reinterpret_cast<LPCDLGTEMPLATEW>(
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, tmplSize + 64));
    if (!pTmpl) return false;

    DLGTEMPLATE* pT = const_cast<DLGTEMPLATE*>(pTmpl);
    pT->style = DS_MODALFRAME | DS_SETFONT | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    pT->cx = 260; pT->cy = 100;

    INT_PTR nRet = DialogBoxIndirectParamW(
        _hInst, pTmpl, _hWnd, _PinDlgProc, reinterpret_cast<LPARAM>(&data));

    HeapFree(GetProcessHeap(), 0, const_cast<DLGTEMPLATE*>(pTmpl));
    return (nRet == IDOK) && data.bOk;
}

// ---------------------------------------------------------------------------
// _PinDlgProc
// ---------------------------------------------------------------------------
INT_PTR CALLBACK WireGuardTrayApp::_PinDlgProc(HWND hDlg, UINT msg,
                                                  WPARAM wParam, LPARAM lParam)
{
    struct PinDlgData { WireGuardTrayApp* pApp; bool bOk; };
    static PinDlgData* s_pData = nullptr;

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        s_pData = reinterpret_cast<PinDlgData*>(lParam);

        // Window title
        SetWindowTextW(hDlg,
            T(L"WireGuard VPN \u2013 YubiKey / Smartcard",
              L"WireGuard VPN \u2013 YubiKey / Smartcard"));

        // Set exact pixel size, then center on screen
        // Dialog template units are unreliable for pixel-precise layout
        const int DLG_W = 370;
        const int DLG_H = 182;  // header(46) + sep + pin(27) + status(14) + sep + buttons(30) + titlebar+border+padding
        int scx = GetSystemMetrics(SM_CXSCREEN);
        int scy = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hDlg, nullptr,
                     (scx - DLG_W) / 2, (scy - DLG_H) / 2,
                     DLG_W, DLG_H,
                     SWP_NOZORDER);

        // Shared fonts
        HFONT hFontBold = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hFontUI = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        // --- Header (y 0-46) ---
        HWND hHdr = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            0, 0, 340, 46, hDlg, reinterpret_cast<HMENU>(101), nullptr, nullptr);
        (void)hHdr;

        HWND hIcon = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_ICON | SS_CENTERIMAGE,
            10, 7, 32, 32, hDlg, reinterpret_cast<HMENU>(102), nullptr, nullptr);
        HICON hIco = static_cast<HICON>(LoadImageW(nullptr,
            MAKEINTRESOURCEW(32516), IMAGE_ICON, 24, 24, LR_SHARED));
        if (!hIco) hIco = LoadIconW(nullptr, IDI_ASTERISK);
        SendMessageW(hIcon, STM_SETICON, reinterpret_cast<WPARAM>(hIco), 0);

        HWND hTitle = CreateWindowExW(0, L"STATIC",
            T(L"YubiKey-Authentifizierung", L"YubiKey Authentication"),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            48, 7, 282, 16, hDlg, reinterpret_cast<HMENU>(103), nullptr, nullptr);
        SendMessageW(hTitle, WM_SETFONT, reinterpret_cast<WPARAM>(hFontBold), TRUE);

        HWND hSub = CreateWindowExW(0, L"STATIC",
            T(L"YubiKey einstecken und PIN eingeben.",
              L"Insert YubiKey and enter PIN."),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            48, 26, 282, 13, hDlg, reinterpret_cast<HMENU>(104), nullptr, nullptr);
        SendMessageW(hSub, WM_SETFONT, reinterpret_cast<WPARAM>(hFontUI), TRUE);

        // --- Separator ---
        CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
            0, 46, 340, 1, hDlg, nullptr, nullptr, nullptr);

        // --- PIN row (y 47-84) ---
        HWND hLbl = CreateWindowExW(0, L"STATIC",
            T(L"PIN:", L"PIN:"),
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
            10, 56, 32, 22, hDlg, nullptr, nullptr, nullptr);
        SendMessageW(hLbl, WM_SETFONT, reinterpret_cast<WPARAM>(hFontUI), TRUE);

        HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL,
            44, 55, 284, 22, hDlg,
            reinterpret_cast<HMENU>(IDC_PIN_EDIT), nullptr, nullptr);
        SendMessageW(hEdit, EM_SETLIMITTEXT, 32, 0);
        SendMessageW(hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(hFontUI), TRUE);

        // Status: small error text between PIN and buttons
        HWND hStatus = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            10, 80, 320, 13, hDlg,
            reinterpret_cast<HMENU>(IDC_SC_STATUS), nullptr, nullptr);
        SendMessageW(hStatus, WM_SETFONT, reinterpret_cast<WPARAM>(hFontUI), TRUE);

        // --- Separator ---
        CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
            0, 96, 340, 1, hDlg, nullptr, nullptr, nullptr);

        // --- Buttons (y 103-127) ---
        HWND hCancel = CreateWindowExW(0, L"BUTTON",
            T(L"Abbrechen", L"Cancel"),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            248, 102, 80, 23, hDlg, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
        SendMessageW(hCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hFontUI), TRUE);

        HWND hOK = CreateWindowExW(0, L"BUTTON",
            T(L"OK", L"OK"),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            162, 102, 80, 23, hDlg, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
        SendMessageW(hOK, WM_SETFONT, reinterpret_cast<WPARAM>(hFontUI), TRUE);

        SetFocus(hEdit);
        return FALSE;
    }
    case WM_CTLCOLORSTATIC:
    {
        // Dark blue header background for controls in header area
        HWND hCtrl = reinterpret_cast<HWND>(lParam);
        RECT rc; GetWindowRect(hCtrl, &rc);
        POINT pt = { rc.left, rc.top };
        ScreenToClient(hDlg, &pt);
        if (pt.y < 46) // in header zone
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkColor(hdc, RGB(0x1a, 0x3a, 0x5c));
            SetTextColor(hdc, RGB(0xFF, 0xFF, 0xFF));
            static HBRUSH hBrHeader = CreateSolidBrush(RGB(0x1a, 0x3a, 0x5c));
            return reinterpret_cast<INT_PTR>(hBrHeader);
        }
        return FALSE;
    }
    case WM_DRAWITEM:
    {
        // Owner-draw header bar
        LPDRAWITEMSTRUCT pDI = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam);
        if (pDI->CtlID == 101)
        {
            HBRUSH hBr = CreateSolidBrush(RGB(0x1a, 0x3a, 0x5c));
            FillRect(pDI->hDC, &pDI->rcItem, hBr);
            DeleteObject(hBr);
        }
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            if (s_pData)
            {
                HWND hEdit = GetDlgItem(hDlg, IDC_PIN_EDIT);
                if (hEdit)
                {
                    GetWindowTextW(hEdit, s_pData->pApp->_wszPin,
                                   ARRAYSIZE(s_pData->pApp->_wszPin));
                    DWORD dwLen = static_cast<DWORD>(wcslen(s_pData->pApp->_wszPin));
                    if (dwLen < s_pData->pApp->_scConfig.dwPinMinLength)
                    {
                        WCHAR wszWarn[128] = {};
                        StringCchPrintfW(wszWarn, ARRAYSIZE(wszWarn),
                            T(L"PIN muss mindestens %lu Zeichen lang sein.",
                              L"PIN must be at least %lu characters."),
                            s_pData->pApp->_scConfig.dwPinMinLength);
                        MessageBoxW(hDlg, wszWarn, L"WireGuard VPN", MB_ICONWARNING | MB_OK);
                        SetFocus(hEdit);
                        return TRUE;
                    }
                }
                s_pData->bOk = true;
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL)
        {
            if (s_pData) s_pData->bOk = false;
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        if (s_pData) s_pData->bOk = false;
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// _WndProc / _HandleMessage
// ---------------------------------------------------------------------------
LRESULT CALLBACK WireGuardTrayApp::_WndProc(HWND hWnd, UINT msg,
                                               WPARAM wParam, LPARAM lParam)
{
    if (g_pApp) return g_pApp->_HandleMessage(hWnd, msg, wParam, lParam);
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT WireGuardTrayApp::_HandleMessage(HWND hWnd, UINT msg,
                                           WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_TRAYICON:
        switch (LOWORD(lParam))
        {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            _RefreshStatus();
            _UpdateTrayIcon();
            _ShowContextMenu();
            break;
        case WM_LBUTTONDBLCLK:
            _RefreshStatus();
            if (_bConnected) _Disconnect();
            else             _Connect(_nSelectedProfile);
            break;
        }
        return 0;

    case WM_COMMAND:
    {
        UINT uCmd = LOWORD(wParam);
        if (uCmd == IDM_CONNECT)    { _Connect(_nSelectedProfile); return 0; }
        if (uCmd == IDM_DISCONNECT) { _Disconnect();               return 0; }
        if (uCmd == IDM_IMPORT)     { _ImportProfile();            return 0; }
        if (uCmd == IDM_OPEN_CONFIG_DIR) { _OpenConfigDir();       return 0; }
        if (uCmd == IDM_EXIT)
        {
            LOG_DEBUG(L"Tray: Exit");
            _RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
        }
        if (uCmd >= IDM_PROFILE_BASE &&
            uCmd < static_cast<UINT>(IDM_PROFILE_BASE + _nProfiles))
        {
            _nSelectedProfile = static_cast<int>(uCmd - IDM_PROFILE_BASE);
            _RefreshStatus();
            _UpdateTrayIcon();
            return 0;
        }
        break;
    }
    case WM_TIMER:
        if (wParam == TIMER_REFRESH_ID)
        {
            _RefreshStatus();
            _UpdateTrayIcon();
            _CheckAndRemoveWireGuardShortcut();
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_REFRESH_ID);
        _RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// _ImportProfile
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_ImportProfile()
{
    LOG_DEBUG(L"Tray: Import profile dialog");

    // Open file picker for .conf selection
    WCHAR wszFile[MAX_PATH_WGCP] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = _hWnd;
    ofn.lpstrFilter = T(L"WireGuard Konfiguration (*.conf)\0*.conf\0Alle Dateien (*.*)\0*.*\0",
                        L"WireGuard Config (*.conf)\0*.conf\0All Files (*.*)\0*.*\0");
    ofn.lpstrFile   = wszFile;
    ofn.nMaxFile    = MAX_PATH_WGCP;
    ofn.lpstrTitle  = T(L"WireGuard Profil importieren", L"Import WireGuard Profile");
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"conf";

    if (!GetOpenFileNameW(&ofn)) { LOG_DEBUG(L"Tray: Import cancelled"); return; }

    // Extract filename from full path
    WCHAR* pName = wcsrchr(wszFile, L'\\');
    PCWSTR pwszFileName = pName ? pName + 1 : wszFile;

    // Build destination path in WireGuard config directory
    WCHAR wszConfigDir[MAX_PATH_WGCP] = {};
    WGGetConfigDir(wszConfigDir, MAX_PATH_WGCP);
    size_t len = wcslen(wszConfigDir);
    if (len > 0 && wszConfigDir[len-1] == L'\\') wszConfigDir[len-1] = L'\0';

    WCHAR wszDest[MAX_PATH_WGCP] = {};
    StringCchPrintfW(wszDest, MAX_PATH_WGCP, L"%s\\%s", wszConfigDir, pwszFileName);

    WCHAR d[MAX_PATH_WGCP * 2] = {};
    StringCchPrintfW(d, ARRAYSIZE(d), L"Tray: Import '%s' -> '%s'", wszFile, wszDest);
    LOG_DEBUG(d);

    // Attempt direct copy first (succeeds if Tray runs elevated)
    if (!CopyFileW(wszFile, wszDest, FALSE))
    {
        // Fallback: elevated copy via cmd.exe with runas verb
        WCHAR wszCmd[MAX_PATH_WGCP * 2] = {};
        StringCchPrintfW(wszCmd, ARRAYSIZE(wszCmd),
                         L"/c copy /Y \"%s\" \"%s\"", wszFile, wszDest);

        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"Tray: CopyFile failed err=%lu, versuche elevated", GetLastError());
        LOG_WARN(e);

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb   = L"runas";
        sei.lpFile   = L"cmd.exe";
        sei.lpParameters = wszCmd;
        sei.nShow    = SW_HIDE;
        sei.fMask    = SEE_MASK_NOCLOSEPROCESS;

        if (!ShellExecuteExW(&sei))
        {
            WCHAR e2[64] = {};
            StringCchPrintfW(e2, 64, L"Tray: Elevated copy failed err=%lu", GetLastError());
            LOG_CRIT(e2);
            MessageBoxW(_hWnd,
                        T(L"Import fehlgeschlagen.\nBitte als Administrator ausf\u00fchren.",
                          L"Import failed.\nPlease run as administrator."),
                        T(L"Import Fehler", L"Import Error"), MB_ICONERROR | MB_OK);
            return;
        }
        if (sei.hProcess)
        {
            WaitForSingleObject(sei.hProcess, 10000);
            CloseHandle(sei.hProcess);
        }
    }

    // Strip extension for success message display
    WCHAR wszProfile[MAX_PATH_WGCP] = {};
    StringCchCopyW(wszProfile, MAX_PATH_WGCP, pwszFileName);
    WCHAR* pExt = wcsrchr(wszProfile, L'.');
    if (pExt) *pExt = L'\0';

    WCHAR wszLog[MAX_PATH_WGCP + 32] = {};
    StringCchPrintfW(wszLog, ARRAYSIZE(wszLog), L"Tray: Import OK: %s", wszProfile);
    LOG_DEBUG(wszLog);

    // Reload profiles – brief pause to let WireGuard process the new file
    Sleep(500);
    _LoadProfiles();
    _RefreshStatus();
    _UpdateTrayIcon();
}


void WireGuardTrayApp::_OpenConfigDir()
{
    WCHAR wszConfigDir[MAX_PATH_WGCP] = {};
    WGGetConfigDir(wszConfigDir, MAX_PATH_WGCP);
    // Trailing backslash entfernen
    size_t len = wcslen(wszConfigDir);
    if (len > 0 && wszConfigDir[len-1] == L'\\') wszConfigDir[len-1] = L'\0';

    // Als Admin oeffnen wegen WireGuard-ACLs
    HINSTANCE hRet = ShellExecuteW(nullptr, L"runas",
                                    L"explorer.exe", wszConfigDir,
                                    nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(hRet) <= 32)
        ShellExecuteW(nullptr, L"explore", wszConfigDir, nullptr, nullptr, SW_SHOWNORMAL);
}

// ---------------------------------------------------------------------------
// WireGuard UI Watcher
// Ueberwacht ob wireguard.exe als UI gestartet wird und killt den Prozess.
// wireguard.exe ohne Parameter = UI -> killen
// wireguard.exe /installtunnelservice = Service -> erlauben
// ---------------------------------------------------------------------------

void WireGuardTrayApp::_StartWireGuardWatcher()
{
    _hWatcherStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!_hWatcherStop) { LOG_WARN(L"Tray: Watcher: CreateEvent failed"); return; }

    _hWatcherThread = CreateThread(nullptr, 0, _WatcherThread, this, 0, nullptr);
    if (!_hWatcherThread)
    {
        LOG_WARN(L"Tray: Watcher: CreateThread failed");
        CloseHandle(_hWatcherStop);
        _hWatcherStop = nullptr;
    }
    else
    {
        LOG_DEBUG(L"Tray: WireGuard UI Watcher gestartet");
    }
}

void WireGuardTrayApp::_StopWireGuardWatcher()
{
    if (_hWatcherStop)
    {
        SetEvent(_hWatcherStop);
    }
    if (_hWatcherThread)
    {
        WaitForSingleObject(_hWatcherThread, 3000);
        CloseHandle(_hWatcherThread);
        _hWatcherThread = nullptr;
    }
    if (_hWatcherStop)
    {
        CloseHandle(_hWatcherStop);
        _hWatcherStop = nullptr;
    }
}

DWORD WINAPI WireGuardTrayApp::_WatcherThread(LPVOID lpParam)
{
    WireGuardTrayApp* pApp = reinterpret_cast<WireGuardTrayApp*>(lpParam);
    LOG_DEBUG(L"Tray: Watcher-Thread laeuft");

    // Check every 500 ms for a wireguard.exe UI instance and terminate it
    while (WaitForSingleObject(pApp->_hWatcherStop, 500) == WAIT_TIMEOUT)
    {
        // Snapshot aller laufenden Prozesse
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) continue;

        PROCESSENTRY32W pe = {};
        pe.dwSize = sizeof(pe);

        if (!Process32FirstW(hSnap, &pe))
        {
            CloseHandle(hSnap);
            continue;
        }

        do
        {
            // Only inspect wireguard.exe processes
            if (_wcsicmp(pe.szExeFile, L"wireguard.exe") != 0) continue;

            DWORD dwPid = pe.th32ProcessID;

            // Open the process for window check and termination
            HANDLE hProc = OpenProcess(
                PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE,
                FALSE, dwPid);
            if (!hProc) continue;

            // Check whether wireguard.exe has a visible window -> UI mode
            // If it runs as /installtunnelservice it has no window and must not be killed
            struct WndCheck { DWORD pid; BOOL found; };
            WndCheck wc = { dwPid, FALSE };
            EnumWindows([](HWND hWnd, LPARAM lp) -> BOOL {
                WndCheck* pwc = reinterpret_cast<WndCheck*>(lp);
                DWORD pid = 0;
                GetWindowThreadProcessId(hWnd, &pid);
                if (pid == pwc->pid && IsWindowVisible(hWnd))
                {
                    pwc->found = TRUE;
                    return FALSE;
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&wc));

            if (wc.found)
            {
                WCHAR d[64] = {};
                StringCchPrintfW(d, 64, L"Tray: WireGuard UI erkannt (PID %lu) - wird beendet", dwPid);
                LOG_WARN(d);
                // Hide all windows immediately before terminating (prevents flash)
                struct HideData { DWORD pid; };
                HideData hd = { dwPid };
                EnumWindows([](HWND hW, LPARAM lp) -> BOOL {
                    DWORD pid = 0;
                    GetWindowThreadProcessId(hW, &pid);
                    if (pid == reinterpret_cast<HideData*>(lp)->pid)
                        ShowWindow(hW, SW_HIDE);
                    return TRUE;
                }, reinterpret_cast<LPARAM>(&hd));
                // Then terminate the process
                TerminateProcess(hProc, 0);
            }

            CloseHandle(hProc);

        } while (Process32NextW(hSnap, &pe));

        CloseHandle(hSnap);
    }

    LOG_DEBUG(L"Tray: Watcher-Thread beendet");
    return 0;
}

// ---------------------------------------------------------------------------
// _CheckAndRemoveWireGuardShortcut
// Prueft bei jedem Timer-Tick ob der WireGuard-Startmenue-Shortcut
// (wieder) angelegt wurde - z.B. nach einem WireGuard-Update.
// Logik:
//   - Shortcut gefunden + kein Backup vorhanden -> Backup anlegen + entfernen
//   - Shortcut gefunden + Backup vorhanden      -> direkt entfernen
//   - Kein Shortcut                             -> nichts tun
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_CheckAndRemoveWireGuardShortcut()
{
    // Startmenue-Pfad ermitteln (alle Benutzer: ProgramData\...\Programs)
    WCHAR wszPrograms[MAX_PATH_WGCP] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS,
                                nullptr, SHGFP_TYPE_CURRENT, wszPrograms)))
        return;

    // Known WireGuard shortcut locations
    WCHAR wszLnk1[MAX_PATH_WGCP] = {};  // directly in Programs
    WCHAR wszLnk2[MAX_PATH_WGCP] = {};  // in Programs\WireGuard subfolder
    StringCchPrintfW(wszLnk1, MAX_PATH_WGCP, L"%s\\WireGuard.lnk",           wszPrograms);
    StringCchPrintfW(wszLnk2, MAX_PATH_WGCP, L"%s\\WireGuard\\WireGuard.lnk", wszPrograms);

    PCWSTR pwszFound = nullptr;
    if (GetFileAttributesW(wszLnk1) != INVALID_FILE_ATTRIBUTES)
        pwszFound = wszLnk1;
    else if (GetFileAttributesW(wszLnk2) != INVALID_FILE_ATTRIBUTES)
        pwszFound = wszLnk2;

    if (!pwszFound)
        return;  // No shortcut found – nothing to do

    // Backup-Pfad: INSTDIR\backup\WireGuard.lnk
    WCHAR wszInstDir[MAX_PATH_WGCP] = {};
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, WGCP_REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        ReadRegString(hKey, WGCP_REG_INSTALLDIR, wszInstDir, MAX_PATH_WGCP, L"");
        RegCloseKey(hKey);
    }

    WCHAR wszBackup[MAX_PATH_WGCP] = {};
    StringCchPrintfW(wszBackup, MAX_PATH_WGCP, L"%s\\backup\\WireGuard.lnk", wszInstDir);

    // Create backup only if it does not yet exist
    if (GetFileAttributesW(wszBackup) == INVALID_FILE_ATTRIBUTES)
    {
        // Ensure backup directory exists
        WCHAR wszBackupDir[MAX_PATH_WGCP] = {};
        StringCchPrintfW(wszBackupDir, MAX_PATH_WGCP, L"%s\\backup", wszInstDir);
        SHCreateDirectoryExW(nullptr, wszBackupDir, nullptr);

        if (CopyFileW(pwszFound, wszBackup, FALSE))
            LOG_DEBUG(L"Tray: WireGuard Shortcut gesichert (nach WireGuard-Update)");
        else
            LOG_WARN(L"Tray: WireGuard Shortcut Backup fehlgeschlagen");
    }

    // Remove shortcut – try direct delete first, fall back to elevated cmd.exe
    if (DeleteFileW(pwszFound))
    {
        LOG_WARN(L"Tray: WireGuard Startmenue-Shortcut entfernt (nach WireGuard-Update)");

        // Remove empty WireGuard subfolder (silently fails if non-empty)
        WCHAR wszDir[MAX_PATH_WGCP] = {};
        StringCchPrintfW(wszDir, MAX_PATH_WGCP, L"%s\\WireGuard", wszPrograms);
        RemoveDirectoryW(wszDir); // schlaegt lautlos fehl wenn nicht leer
    }
    else
    {
        // Direct delete failed (ACL) – retry elevated via cmd.exe runas
        WCHAR wszCmd[MAX_PATH_WGCP + 32] = {};
        StringCchPrintfW(wszCmd, ARRAYSIZE(wszCmd), L"/c del /f /q \"%s\"", pwszFound);
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb       = L"runas";
        sei.lpFile       = L"cmd.exe";
        sei.lpParameters = wszCmd;
        sei.nShow        = SW_HIDE;
        if (ShellExecuteExW(&sei))
        {
            if (sei.hProcess) { WaitForSingleObject(sei.hProcess, 3000); CloseHandle(sei.hProcess); }
            LOG_WARN(L"Tray: WireGuard Shortcut elevated entfernt");
        }
    }
}

// ---------------------------------------------------------------------------
// Smartcard presence watcher
// Monitors card insert/remove events and triggers auto-connect/disconnect.
// Runs as a background thread, checks card presence every second.
// ---------------------------------------------------------------------------

void WireGuardTrayApp::_StartSmartcardWatcher()
{
    _hScWatchStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!_hScWatchStop) { LOG_WARN(L"SC Watcher: CreateEvent failed"); return; }

    _hScWatchThread = CreateThread(nullptr, 0, _SmartcardWatchThread, this, 0, nullptr);
    if (!_hScWatchThread)
    {
        LOG_WARN(L"SC Watcher: CreateThread failed");
        CloseHandle(_hScWatchStop);
        _hScWatchStop = nullptr;
    }
    else
    {
        LOG_DEBUG(L"Tray: Smartcard watcher started");
    }
}

void WireGuardTrayApp::_StopSmartcardWatcher()
{
    if (_hScWatchStop)  SetEvent(_hScWatchStop);
    if (_hScWatchThread)
    {
        WaitForSingleObject(_hScWatchThread, 5000);
        CloseHandle(_hScWatchThread);
        _hScWatchThread = nullptr;
    }
    if (_hScWatchStop)
    {
        CloseHandle(_hScWatchStop);
        _hScWatchStop = nullptr;
    }
}

DWORD WINAPI WireGuardTrayApp::_SmartcardWatchThread(LPVOID lpParam)
{
    WireGuardTrayApp* pApp = reinterpret_cast<WireGuardTrayApp*>(lpParam);
    LOG_DEBUG(L"SC Watcher: Thread running");

    bool bCardWasPresentLastTick = false;
    WCHAR wszReader[256] = {};

    while (WaitForSingleObject(pApp->_hScWatchStop, 1000) == WAIT_TIMEOUT)
    {
        // Check whether a PIV card is currently present
        WCHAR wszFoundReader[256] = {};
        bool bCardPresent = WGCPFindSmartcard(pApp->_scConfig, wszFoundReader, 256);

        if (bCardPresent && !bCardWasPresentLastTick)
        {
            // Card just inserted
            StringCchCopyW(wszReader, 256, wszFoundReader);
            LOG_DEBUG(L"SC Watcher: Card inserted");

            if (pApp->_scConfig.bConnectOnInsert && !pApp->_bConnected
                && pApp->_nProfiles > 0)
            {
                LOG_DEBUG(L"SC Watcher: Auto-connect triggered");
                // Post connect command to main window thread (thread-safe)
                PostMessageW(pApp->_hWnd, WM_COMMAND,
                             MAKEWPARAM(IDM_CONNECT, 0), 0);
            }
        }
        else if (!bCardPresent && bCardWasPresentLastTick)
        {
            // Card just removed
            LOG_DEBUG(L"SC Watcher: Card removed");

            if (pApp->_scConfig.bDisconnectOnRemove && pApp->_bConnected)
            {
                LOG_DEBUG(L"SC Watcher: Auto-disconnect triggered");
                PostMessageW(pApp->_hWnd, WM_COMMAND,
                             MAKEWPARAM(IDM_DISCONNECT, 0), 0);
            }
            ZeroMemory(wszReader, sizeof(wszReader));
        }

        bCardWasPresentLastTick = bCardPresent;
    }

    LOG_DEBUG(L"SC Watcher: Thread stopped");
    return 0;
}
