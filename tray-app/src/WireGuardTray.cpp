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
    ZeroMemory(_wszYkMgrPath, sizeof(_wszYkMgrPath));
    _hScWatchStop    = nullptr;
    _hNetWatchThread = nullptr;
    _hNetWatchStop   = nullptr;
    ZeroMemory(&_nid,              sizeof(_nid));
    ZeroMemory(&_scConfig,         sizeof(_scConfig));
    ZeroMemory(_wszExePath,        sizeof(_wszExePath));
    ZeroMemory(_wszWgExePath,      sizeof(_wszWgExePath));
    ZeroMemory(_wszCurrentProfile, sizeof(_wszCurrentProfile));
    ZeroMemory(_rgProfiles,        sizeof(_rgProfiles));
    ZeroMemory(_wszPin,            sizeof(_wszPin));
    ZeroMemory(_wszScStatusMsg,    sizeof(_wszScStatusMsg));
    ZeroMemory(_wszYkSerial,       sizeof(_wszYkSerial));
    _bAutoUpdateCheck     = false;
    _bUpdateBalloonActive = false;
    ZeroMemory(_wszUpdateUrl, sizeof(_wszUpdateUrl));
    _hUpdateThread        = nullptr;
    _hUpdateStop          = nullptr;
}

WireGuardTrayApp::~WireGuardTrayApp()
{
    _StopWireGuardWatcher();
    _StopSmartcardWatcher();
    _StopNetworkWatcher();
    _StopUpdateCheckThread();
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

    _DisableWireGuardManager();
    _StartWireGuardWatcher();
    if (_scConfig.bEnabled &&
        (_scConfig.bConnectOnInsert || _scConfig.bDisconnectOnRemove))
        _StartSmartcardWatcher();
    _StartNetworkWatcher();
    if (_bAutoUpdateCheck) _StartUpdateCheckThread();
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
        _dwHandshakeTimeoutSec = ReadRegDword(hKey, WGCP_REG_HANDSHAKE_TIMEOUT_SEC, 0);
        _bAutoUpdateCheck = (ReadRegDword(hKey, L"AutoUpdateCheck", 1) != 0); // default: an
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
    if (_nProfiles == 0)
    {
        _bConnected = false;
        return;
    }

    // Clamp stale index
    if (_nSelectedProfile >= _nProfiles)
        _nSelectedProfile = 0;

    // Check the currently selected profile first (fast path).
    PCWSTR pwszProfile = _rgProfiles[_nSelectedProfile];
    _bConnected = WGIsTunnelConnected(pwszProfile);
    StringCchCopyW(_wszCurrentProfile, MAX_PATH_WGCP, pwszProfile);

    // If the selected profile is not connected, scan ALL profiles to detect a
    // tunnel that the Credential Provider may have started from the lock screen.
    // This fixes the bug where the tray stays red after a pre-logon VPN connect.
    if (!_bConnected)
    {
        for (int i = 0; i < _nProfiles; i++)
        {
            if (i == _nSelectedProfile)
                continue;
            if (WGIsTunnelConnected(_rgProfiles[i]))
            {
                _nSelectedProfile = i;
                _bConnected = true;
                StringCchCopyW(_wszCurrentProfile, MAX_PATH_WGCP, _rgProfiles[i]);
                LOG_DEBUG(L"_RefreshStatus: active tunnel found via profile scan, updated selected profile");
                break;
            }
        }
    }
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
    PCWSTR pwszProfile = (_nProfiles > 0) ? _rgProfiles[_nSelectedProfile] : L"";

    if (_bConnected && _nProfiles > 0)
    {
        // Traffic stats
        WCHAR wszTraffic[MAX_LABEL_WGCP] = {};
        WGGetTrafficStats(_wszWgExePath, pwszProfile, wszTraffic, MAX_LABEL_WGCP);

        // Connection duration
        WCHAR wszTimer[MAX_LABEL_WGCP] = {};
        WGGetConnectedSince(pwszProfile, wszTimer, MAX_LABEL_WGCP);

        // Last handshake age
        WCHAR wszHandshake[64] = {};
        LONGLONG llAge = WGGetLastHandshakeSec(_wszWgExePath, pwszProfile);
        if (llAge >= 0)
        {
            LONGLONG h = llAge/3600, m = (llAge%3600)/60, s = llAge%60;
            if (h > 0)
                StringCchPrintfW(wszHandshake, 64,
                    T(L"\U0001F511 Handshake vor %lldh %lldm",
                      L"\U0001F511 Handshake %lldh %lldm ago"),
                    h, m);
            else if (m > 0)
                StringCchPrintfW(wszHandshake, 64,
                    T(L"\U0001F511 Handshake vor %lldm %llds",
                      L"\U0001F511 Handshake %lldm %llds ago"),
                    m, s);
            else
                StringCchPrintfW(wszHandshake, 64,
                    T(L"\U0001F511 Handshake vor %llds",
                      L"\U0001F511 Handshake %llds ago"),
                    s);
        }
        else
            StringCchCopyW(wszHandshake, 64,
                T(L"\U0001F511 Handshake ausstehend",
                  L"\U0001F511 Handshake pending"));

        // Build tooltip: max 127 chars (Windows tray limit)
        // Line 1: app name + status
        // Line 2: profile
        // Line 3: connection duration
        // Line 4: handshake age
        // Line 5: traffic (if available)
        // Tooltip layout (all lines left-aligned with emoji prefix):
        // WireGuard VPN
        // 🟢 Verbunden
        // 🖥 LT260430
        // ⏱ Verbunden seit 04:58:47
        // 🔑 Handshake  vor 45s
        // 🌐 ↑ 78.9 MB  ↓ 42.5 MB
        PCWSTR pwszUptime = wszTimer[0]
            ? wszTimer
            : T(L"\u23F1 Laufzeit  unbekannt", L"\u23F1 Uptime  unknown");

        if (wszTraffic[0])
            StringCchPrintfW(_nid.szTip, ARRAYSIZE(_nid.szTip),
                L"WireGuard VPN\n"
                L"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n"
                L"\U0001F7E2 %s\n"
                L"\U0001F5A5 %s\n"
                L"%s\n"
                L"%s\n"
                L"\U0001F310 %s",
                T(L"Verbunden", L"Connected"),
                pwszProfile,
                pwszUptime,
                wszHandshake,
                wszTraffic);
        else
            StringCchPrintfW(_nid.szTip, ARRAYSIZE(_nid.szTip),
                L"WireGuard VPN\n"
                L"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n"
                L"\U0001F7E2 %s\n"
                L"\U0001F5A5 %s\n"
                L"%s\n"
                L"%s",
                T(L"Verbunden", L"Connected"),
                pwszProfile,
                pwszUptime,
                wszHandshake);
    }
    else
    {
        if (_nProfiles == 0)
        {
            // Kein Profil: Nutzer zur Aktion leiten
            StringCchPrintfW(_nid.szTip, ARRAYSIZE(_nid.szTip),
                L"WireGuard VPN\n"
                L"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n"
                L"\U000026A0 %s\n"
                L"%s",
                T(L"Kein Profil vorhanden",
                  L"No profile configured"),
                T(L"Klicken zum Importieren...",
                  L"Click to import a profile..."));
        }
        else
        {
            StringCchPrintfW(_nid.szTip, ARRAYSIZE(_nid.szTip),
                L"WireGuard VPN\n"
                L"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n"
                L"\U0001F534 %s\n"
                L"\U0001F5A5 %s\n"
                L"%s",
                T(L"Getrennt", L"Disconnected"),
                pwszProfile,
                T(L"Klicken zum Verbinden",
                  L"Click to connect"));
        }
    }
}

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_ShowContextMenu()
{
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // -----------------------------------------------------------------------
    // Header: App-Name (ausgegraut)
    // -----------------------------------------------------------------------
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0,
        L"\U0001F512  WireGuard VPN");

    // -----------------------------------------------------------------------
    // Status-Zeile: Verbindungsstatus + Profilname + ggf. Laufzeit
    // -----------------------------------------------------------------------
    {
        WCHAR wszStatus[MAX_PATH_WGCP + 128] = {};
        if (_nProfiles > 0)
        {
            if (_bConnected)
            {
                WCHAR wszTimer[MAX_LABEL_WGCP] = {};
                WGGetConnectedSince(_rgProfiles[_nSelectedProfile], wszTimer, MAX_LABEL_WGCP);
                if (wszTimer[0])
                    StringCchPrintfW(wszStatus, ARRAYSIZE(wszStatus),
                        T(L"\U0001F7E2 Verbunden  \u2013  %s", L"\U0001F7E2 Connected  \u2013  %s"),
                        wszTimer);
                else
                    StringCchPrintfW(wszStatus, ARRAYSIZE(wszStatus),
                        T(L"\U0001F7E2 Verbunden  \u2013  %s", L"\U0001F7E2 Connected  \u2013  %s"),
                        _rgProfiles[_nSelectedProfile]);
            }
            else
            {
                StringCchPrintfW(wszStatus, ARRAYSIZE(wszStatus),
                    T(L"\U0001F534 Getrennt  \u2013  %s", L"\U0001F534 Disconnected  \u2013  %s"),
                    _rgProfiles[_nSelectedProfile]);
            }
        }
        else
        {
            StringCchCopyW(wszStatus, ARRAYSIZE(wszStatus),
                T(L"\u26A0  Kein Profil vorhanden", L"\u26A0  No profile configured"));
        }
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, wszStatus);
    }
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // -----------------------------------------------------------------------
    // Haupt-Aktion: Verbinden / Trennen – mit Profilname damit klar ist was passiert
    // -----------------------------------------------------------------------
    if (_nProfiles > 0)
    {
        if (_bConnected)
        {
            // Profilname im Label: Nutzer sieht WAS getrennt wird
            WCHAR wszLabel[MAX_PATH_WGCP + 32] = {};
            StringCchPrintfW(wszLabel, ARRAYSIZE(wszLabel),
                T(L"\u23F9  VPN trennen  \u2013  %s", L"\u23F9  Disconnect VPN  \u2013  %s"),
                _rgProfiles[_nSelectedProfile]);
            AppendMenuW(hMenu, MF_STRING, IDM_DISCONNECT, wszLabel);
        }
        else
        {
            WCHAR wszLabel[MAX_PATH_WGCP + 32] = {};
            StringCchPrintfW(wszLabel, ARRAYSIZE(wszLabel),
                T(L"\u25B6  VPN verbinden  \u2013  %s", L"\u25B6  Connect VPN  \u2013  %s"),
                _rgProfiles[_nSelectedProfile]);
            AppendMenuW(hMenu, MF_STRING, IDM_CONNECT, wszLabel);
        }
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    }

    // -----------------------------------------------------------------------
    // Profil-Liste
    //
    // Jedes Profil ist ein MF_POPUP-Eintrag. Das Submenu zeigt kontextsensitiv:
    //   Verbundenes Profil:    [Trennen | -- | Loeschen (grayed)]
    //   Anderes Profil (frei): [Verbinden | Als Standard | -- | Loeschen]
    //   Anderes Profil (busy): [Wechseln zu X | Als Standard | -- | Loeschen]
    //
    // Löschen ist nur gesperrt wenn dieses Profil GERADE VERBUNDEN ist.
    // Ausgewähltes-aber-getrenntes Profil kann gelöscht werden.
    //
    // Parent-Eintrag: MF_CHECKED wenn ausgewählt, grüner Punkt wenn verbunden.
    // -----------------------------------------------------------------------
    for (int i = 0; i < _nProfiles; i++)
    {
        bool bIsSelected     = (i == _nSelectedProfile);
        bool bIsConnected    = bIsSelected && _bConnected;
        bool bOtherConnected = _bConnected && !bIsSelected;

        HMENU hSub = CreatePopupMenu();
        if (!hSub) { LOG_WARN(L"Menu: CreatePopupMenu for profile submenu failed"); continue; }

        // --- Submenu-Eintrag: Verbinden / Trennen / Wechseln ---
        if (bIsConnected)
        {
            AppendMenuW(hSub, MF_STRING, IDM_DISCONNECT,
                T(L"\u23F9  Trennen", L"\u23F9  Disconnect"));
        }
        else if (bOtherConnected)
        {
            // Anderes Profil ist aktiv -> direkter Wechsel anbieten
            WCHAR wszSwitch[MAX_PATH_WGCP + 32] = {};
            StringCchPrintfW(wszSwitch, ARRAYSIZE(wszSwitch),
                T(L"\u21C4  Wechseln zu %s", L"\u21C4  Switch to %s"),
                _rgProfiles[i]);
            AppendMenuW(hSub, MF_STRING,
                static_cast<UINT_PTR>(IDM_PROFILE_SWITCH_BASE + i), wszSwitch);
        }
        else
        {
            // Nichts verbunden – direkt verbinden
            AppendMenuW(hSub, MF_STRING,
                static_cast<UINT_PTR>(IDM_PROFILE_CONNECT_BASE + i),
                T(L"\u25B6  Verbinden", L"\u25B6  Connect"));
        }

        // --- Submenu-Eintrag: Als Standard auswaehlen (nur wenn nicht bereits aktiv) ---
        if (!bIsSelected)
        {
            AppendMenuW(hSub, MF_STRING,
                static_cast<UINT_PTR>(IDM_PROFILE_SELECT_BASE + i),
                T(L"\u2714  Als Standard ausw\u00E4hlen",
                  L"\u2714  Set as default"));
        }

        AppendMenuW(hSub, MF_SEPARATOR, 0, nullptr);

                // --- Submenu-Eintrag: Bearbeiten ---
        AppendMenuW(hSub, MF_STRING,
            static_cast<UINT_PTR>(IDM_PROFILE_EDIT_BASE + i),
            T(L"\u270F  Bearbeiten...", L"\u270F  Edit..."));

        // --- Submenu-Eintrag: Exportieren ---
        AppendMenuW(hSub, MF_STRING,
            static_cast<UINT_PTR>(IDM_PROFILE_EXPORT_BASE + i),
            T(L"\U0001F4BE  Exportieren...", L"\U0001F4BE  Export..."));

// --- Submenu-Eintrag: Loeschen (nur gesperrt wenn gerade verbunden) ---
        UINT uDelFlags = MF_STRING;
        if (bIsConnected) uDelFlags |= MF_GRAYED;  // erst trennen, dann loeschen
        AppendMenuW(hSub, uDelFlags,
            static_cast<UINT_PTR>(IDM_PROFILE_DELETE_BASE + i),
            T(L"\U0001F5D1  L\u00F6schen", L"\U0001F5D1  Delete"));

        // --- Parent-Eintrag: Profilname ---
        // Grüner Punkt wenn verbunden, Pfeil wenn ausgewählt (aber nicht verbunden), sonst Abstand
        WCHAR wszProfEntry[MAX_PATH_WGCP + 8] = {};
        if (bIsConnected)
            StringCchPrintfW(wszProfEntry, ARRAYSIZE(wszProfEntry), L"\U0001F7E2 %s", _rgProfiles[i]);
        else if (bIsSelected)
            StringCchPrintfW(wszProfEntry, ARRAYSIZE(wszProfEntry), L"\u25B8  %s", _rgProfiles[i]);
        else
            StringCchPrintfW(wszProfEntry, ARRAYSIZE(wszProfEntry), L"    %s", _rgProfiles[i]);

        UINT uFlags = MF_POPUP;
        if (bIsSelected) uFlags |= MF_CHECKED;

        AppendMenuW(hMenu, uFlags, reinterpret_cast<UINT_PTR>(hSub), wszProfEntry);
    }

    if (_nProfiles > 0 || _scConfig.bEnabled)
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    // -----------------------------------------------------------------------
    // YubiKey / Smartcard-Status (nur wenn aktiviert)
    // Seriennummer wird im SC-Watcher-Thread gecacht – kein blockierender
    // ykman-Aufruf beim Menueöffnen mehr.
    // -----------------------------------------------------------------------
    if (_scConfig.bEnabled)
    {
        WCHAR wszReader[256] = {};
        bool bYkPresent = WGCPFindSmartcard(_scConfig, wszReader, 256);

        WCHAR wszYkLine[128] = {};
        if (bYkPresent)
        {
            if (_wszYkSerial[0])
                StringCchPrintfW(wszYkLine, 128,
                    T(L"\U0001F511  YubiKey verbunden  (S/N %s)",
                      L"\U0001F511  YubiKey connected  (S/N %s)"),
                    _wszYkSerial);
            else
                StringCchCopyW(wszYkLine, 128,
                    T(L"\U0001F511  YubiKey verbunden",
                      L"\U0001F511  YubiKey connected"));
        }
        else
        {
            // Karte nicht mehr da: Serial-Cache leeren
            ZeroMemory(_wszYkSerial, sizeof(_wszYkSerial));
            StringCchCopyW(wszYkLine, 128,
                T(L"\U0001F511  YubiKey nicht erkannt",
                  L"\U0001F511  YubiKey not detected"));
        }
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, wszYkLine);

        // YubiKey Manager / Authenticator oeffnen (gecachter Pfad oder einmalige Suche)
        if (!_wszYkMgrPath[0])
        {
            const WCHAR* apwszPaths[] = {
                L"%PROGRAMFILES%\\Yubico\\Yubico Authenticator\\authenticator.exe",
                L"%PROGRAMFILES(X86)%\\Yubico\\Yubico Authenticator\\authenticator.exe",
                L"%LOCALAPPDATA%\\Programs\\Yubico Authenticator\\authenticator.exe",
                L"%PROGRAMFILES%\\Yubico\\YubiKey Manager\\ykman-gui.exe",
                L"%PROGRAMFILES(X86)%\\Yubico\\YubiKey Manager\\ykman-gui.exe",
                L"%LOCALAPPDATA%\\Programs\\yubikey-manager-qt\\ykman-gui.exe",
            };
            WCHAR wszTry[MAX_PATH] = {};
            for (auto pwszP : apwszPaths)
            {
                ExpandEnvironmentStringsW(pwszP, wszTry, MAX_PATH);
                if (GetFileAttributesW(wszTry) != INVALID_FILE_ATTRIBUTES)
                {
                    StringCchCopyW(_wszYkMgrPath, MAX_PATH, wszTry);
                    break;
                }
            }
        }
        if (_wszYkMgrPath[0])
            AppendMenuW(hMenu, MF_STRING, IDM_OPEN_YKMANAGER,
                T(L"\U0001F511  Yubico Authenticator \u00F6ffnen...",
                  L"\U0001F511  Open Yubico Authenticator..."));

        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    }

    // -----------------------------------------------------------------------
    // Profil-Verwaltung
    // -----------------------------------------------------------------------
    AppendMenuW(hMenu, MF_STRING, IDM_IMPORT,
        T(L"\U0001F4C2  Profil importieren...",
          L"\U0001F4C2  Import profile..."));
    AppendMenuW(hMenu, MF_STRING, IDM_OPEN_CONFIG_DIR,
        T(L"\U0001F4C1  Konfigurationsordner \u00F6ffnen...",
          L"\U0001F4C1  Open config folder..."));

    AppendMenuW(hMenu, (_bAutoUpdateCheck ? MF_CHECKED : MF_UNCHECKED) | MF_STRING,
        IDM_UPDATE_CHECK,
        T(L"\U0001F504  Auf Updates pr\u00FCfen", L"\U0001F504  Check for updates"));
    AppendMenuW(hMenu, MF_STRING, IDM_ABOUT,
        T(L"\u2139  Informationen...", L"\u2139  About..."));
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT,
        T(L"❌  Beenden", L"❌  Exit"));

    // -----------------------------------------------------------------------
    // Menue anzeigen
    // -----------------------------------------------------------------------
    SetForegroundWindow(_hWnd);
    POINT pt = {};
    GetCursorPos(&pt);

    // Dark Mode
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
    if (dwLight == 0)
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
// _ShowBalloon – helper for tray balloon notifications
// dwTimeout: display duration in ms (Windows caps at ~30 s; 0 = system default ~4 s)
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_ShowBalloon(PCWSTR pwszTitle, PCWSTR pwszMsg,
                                     DWORD dwInfoFlags, DWORD dwTimeout)
{
    NOTIFYICONDATAW nid = { sizeof(nid) };
    nid.hWnd        = _hWnd;
    nid.uID         = 1;
    nid.uFlags      = NIF_INFO;
    nid.dwInfoFlags = dwInfoFlags;
    nid.uTimeout    = (dwTimeout > 0) ? dwTimeout : 4000;
    StringCchCopyW(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), pwszTitle);
    StringCchCopyW(nid.szInfo,      ARRAYSIZE(nid.szInfo),      pwszMsg);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

// ---------------------------------------------------------------------------
// _SelectProfile – Profil aktiv setzen OHNE eine Verbindung herzustellen.
// Wird aufgerufen wenn der Nutzer ein Profil im Kontextmenü auswählt.
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_SelectProfile(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= _nProfiles) return;
    if (profileIndex == _nSelectedProfile)
    {
        LOG_DEBUG(L"Tray: SelectProfile - profile already selected, no change");
        return;
    }

    WCHAR d[MAX_PATH_WGCP + 64] = {};
    StringCchPrintfW(d, ARRAYSIZE(d),
        L"Tray: SelectProfile[%d] '%s' (was [%d] '%s')",
        profileIndex, _rgProfiles[profileIndex],
        _nSelectedProfile, _rgProfiles[_nSelectedProfile]);
    LOG_DEBUG(d);

    _nSelectedProfile = profileIndex;
    _RefreshStatus();
    _UpdateTrayIcon();

    // Kurze Balloon-Rückmeldung damit der Nutzer sieht was ausgewählt wurde
    WCHAR wszMsg[MAX_PATH_WGCP + 64] = {};
    StringCchPrintfW(wszMsg, ARRAYSIZE(wszMsg),
        T(L"Profil \u201e%s\u201c ausgew\u00E4hlt.\nMit VPN verbinden um zu aktivieren.",
          L"Profile \u201c%s\u201d selected.\nClick Connect VPN to activate."),
        _rgProfiles[_nSelectedProfile]);
    _ShowBalloon(L"WireGuard VPN", wszMsg, NIIF_INFO, 4000);
}

// ---------------------------------------------------------------------------
// _SwitchProfile – aktiven Tunnel trennen, dann ein anderes Profil verbinden.
// Wird aufgerufen wenn der Nutzer "Wechseln zu X" im Submenu wählt.
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_SwitchProfile(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= _nProfiles) return;
    if (profileIndex == _nSelectedProfile && _bConnected)
    {
        LOG_DEBUG(L"Tray: SwitchProfile - profile already connected, no change");
        return;
    }

    WCHAR d[MAX_PATH_WGCP + 64] = {};
    StringCchPrintfW(d, ARRAYSIZE(d),
        L"Tray: SwitchProfile - disconnect '%s', connect '%s'",
        _rgProfiles[_nSelectedProfile], _rgProfiles[profileIndex]);
    LOG_DEBUG(d);

    // 1. Aktuellen Tunnel trennen
    _Disconnect();

    // 2. Neues Profil verbinden
    _Connect(profileIndex);
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

    WGConnect(_wszExePath, pwszProfile);

    bool bOk = false;
    for (int i = 0; i < 12; i++)
    {
        Sleep(500);
        if (WGIsTunnelConnected(pwszProfile)) { bOk = true; break; }
    }

    _RefreshStatus();
    _UpdateTrayIcon();

    if (bOk)
    {
        WCHAR wszMsg[MAX_PATH_WGCP + 32] = {};
        StringCchPrintfW(wszMsg, ARRAYSIZE(wszMsg),
            T(L"Tunnel '%s' verbunden.", L"Tunnel '%s' connected."),
            pwszProfile);
        _ShowBalloon(L"WireGuard VPN", wszMsg, NIIF_INFO);
        LOG_DEBUG(wszMsg);
    }
    else
    {
        WCHAR wszMsg[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(wszMsg, ARRAYSIZE(wszMsg),
            T(L"Tunnel '%s' konnte nicht gestartet werden.",
              L"Tunnel '%s' could not be started."),
            pwszProfile);
        _ShowBalloon(L"WireGuard VPN", wszMsg, NIIF_WARNING);
        LOG_WARN(wszMsg);
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

    bool bStopped = false;
    for (int i = 0; i < 12; i++)
    {
        Sleep(500);
        if (!WGIsTunnelConnected(pwszProfile)) { bStopped = true; break; }
    }
    _RefreshStatus();
    _UpdateTrayIcon();

    WCHAR wszMsg[128] = {};
    if (bStopped)
    {
        StringCchPrintfW(wszMsg, ARRAYSIZE(wszMsg),
            T(L"Tunnel '%s' getrennt.", L"Tunnel '%s' disconnected."),
            pwszProfile);
        _ShowBalloon(L"WireGuard VPN", wszMsg, NIIF_INFO);
        LOG_DEBUG(wszMsg);
    }
    else
    {
        StringCchPrintfW(wszMsg, ARRAYSIZE(wszMsg),
            T(L"Tunnel '%s' konnte nicht getrennt werden.",
              L"Tunnel '%s' could not be disconnected."),
            pwszProfile);
        _ShowBalloon(L"WireGuard VPN", wszMsg, NIIF_WARNING);
        LOG_WARN(wszMsg);
    }
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
        _ShowBalloon(
            T(L"WireGuard VPN \u2013 YubiKey-Authentifizierung",
              L"WireGuard VPN \u2013 YubiKey Authentication"),
            wszMsg, NIIF_WARNING);

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

        case NIN_BALLOONUSERCLICK:
            if (_bUpdateBalloonActive && _wszUpdateUrl[0])
            {
                ShellExecuteW(nullptr, L"open", _wszUpdateUrl, nullptr, nullptr, SW_SHOWNORMAL);
                _bUpdateBalloonActive = false;
            }
            break;

        case WM_LBUTTONDBLCLK:
            // Doppelklick: Verbinden / Trennen (Toggle)
            // WM_LBUTTONUP wird von Windows VOR WM_LBUTTONDBLCLK gesendet.
            // Um den Doppelklick-Bug zu vermeiden (zweifaches Auslösen),
            // nutzen wir einen Timer: beim ersten LBUTTONUP setzen wir einen
            // kurzen Timer (300ms). Wenn innerhalb dieser Zeit ein LBUTTONDBLCLK
            // kommt, canceln wir den Timer und führen die Aktion nur einmal aus.
            // Implementierung: Doppelklick führt direkt aus, LBUTTONUP prüft ob
            // ein Doppelklick folgt. Wir nutzen GetDoubleClickTime() dafür.
            KillTimer(hWnd, 2);  // Cancel pending single-click timer
            _RefreshStatus();
            if (_nProfiles == 0)
            {
                // Kein Profil: Importdialog öffnen
                _ImportProfile();
            }
            else if (_bConnected)
            {
                _Disconnect();
            }
            else
            {
                _Connect(_nSelectedProfile);
            }
            break;

        case WM_LBUTTONUP:
            // Single-click: verzögert ausführen um Doppelklick zu erkennen.
            // SetTimer mit GetDoubleClickTime() – wenn kein Doppelklick folgt,
            // wird WM_TIMER (ID=2) ausgelöst und die Aktion ausgeführt.
            SetTimer(hWnd, 2, GetDoubleClickTime(), nullptr);
            break;
        }
        return 0;

    case WM_COMMAND:
    {
        UINT uCmd = LOWORD(wParam);
        if (uCmd == IDM_CONNECT)        { _Connect(_nSelectedProfile); return 0; }
        if (uCmd == IDM_DISCONNECT)     { _Disconnect();               return 0; }
        if (uCmd == IDM_IMPORT)         { _ImportProfile();            return 0; }
        if (uCmd == IDM_DELETE_PROFILE) { _DeleteProfile();            return 0; } // legacy fallback
        if (uCmd >= IDM_PROFILE_EDIT_BASE &&
            uCmd <  static_cast<UINT>(IDM_PROFILE_EDIT_BASE + _nProfiles))
        {
            int iEdit = static_cast<int>(uCmd - IDM_PROFILE_EDIT_BASE);
            _EditProfile(iEdit);
            return 0;
        }
        if (uCmd >= IDM_PROFILE_EXPORT_BASE &&
            uCmd <  static_cast<UINT>(IDM_PROFILE_EXPORT_BASE + _nProfiles))
        {
            int iExp = static_cast<int>(uCmd - IDM_PROFILE_EXPORT_BASE);
            _ExportProfile(iExp);
            return 0;
        }
        if (uCmd >= IDM_PROFILE_DELETE_BASE &&
            uCmd <  static_cast<UINT>(IDM_PROFILE_DELETE_BASE + _nProfiles))
        {
            int iDel = static_cast<int>(uCmd - IDM_PROFILE_DELETE_BASE);
            WCHAR dbg[64] = {};
            StringCchPrintfW(dbg, 64, L"Tray: delete requested for profile[%d]", iDel);
            LOG_DEBUG(dbg);
            _DeleteProfileAt(iDel);
            return 0;
        }
        if (uCmd == IDM_OPEN_YKMANAGER) { _OpenYubiKeyManager();       return 0; }
        if (uCmd == IDM_OPEN_CONFIG_DIR){ _OpenConfigDir();            return 0; }
        if (uCmd == IDM_ABOUT)          { _ShowAboutDialog();           return 0; }
        if (uCmd == IDM_UPDATE_CHECK)
        {
            _bAutoUpdateCheck = !_bAutoUpdateCheck;
            // In Registry schreiben
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, WGCP_REG_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
            {
                DWORD dw = _bAutoUpdateCheck ? 1 : 0;
                RegSetValueExW(hKey, L"AutoUpdateCheck", 0, REG_DWORD, (BYTE*)&dw, sizeof(dw));
                RegCloseKey(hKey);
            }
            if (_bAutoUpdateCheck)
                _StartUpdateCheckThread();
            else
                _StopUpdateCheckThread();
            return 0;
        }
        if (uCmd == IDM_EXIT)
        {
            LOG_DEBUG(L"Tray: Exit");
            _RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
        }
        // Profil direkt verbinden aus Submenu (IDM_PROFILE_CONNECT_BASE + i).
        if (uCmd >= IDM_PROFILE_CONNECT_BASE &&
            uCmd < static_cast<UINT>(IDM_PROFILE_CONNECT_BASE + _nProfiles))
        {
            int iConn = static_cast<int>(uCmd - IDM_PROFILE_CONNECT_BASE);
            WCHAR dbg[64] = {};
            StringCchPrintfW(dbg, 64, L"Tray: connect profile[%d]", iConn);
            LOG_DEBUG(dbg);
            _Connect(iConn);
            return 0;
        }
        // Profil als Standard setzen OHNE Verbinden (IDM_PROFILE_SELECT_BASE + i).
        if (uCmd >= IDM_PROFILE_SELECT_BASE &&
            uCmd < static_cast<UINT>(IDM_PROFILE_SELECT_BASE + _nProfiles))
        {
            int iSel = static_cast<int>(uCmd - IDM_PROFILE_SELECT_BASE);
            WCHAR dbg[64] = {};
            StringCchPrintfW(dbg, 64, L"Tray: select profile[%d] as default", iSel);
            LOG_DEBUG(dbg);
            _SelectProfile(iSel);
            return 0;
        }
        // Profil wechseln: aktiven Tunnel trennen, neues Profil verbinden
        // (IDM_PROFILE_SWITCH_BASE + i).
        if (uCmd >= IDM_PROFILE_SWITCH_BASE &&
            uCmd < static_cast<UINT>(IDM_PROFILE_SWITCH_BASE + _nProfiles))
        {
            int iSwitch = static_cast<int>(uCmd - IDM_PROFILE_SWITCH_BASE);
            WCHAR dbg[64] = {};
            StringCchPrintfW(dbg, 64, L"Tray: switch to profile[%d]", iSwitch);
            LOG_DEBUG(dbg);
            _SwitchProfile(iSwitch);
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
        else if (wParam == 2)
        {
            // Single-click timer abgelaufen (kein Doppelklick folgte):
            // Verbinden / Trennen ausführen
            KillTimer(hWnd, 2);
            _RefreshStatus();
            if (_nProfiles == 0)
            {
                _ImportProfile();
            }
            else if (_bConnected)
            {
                _Disconnect();
            }
            else
            {
                _Connect(_nSelectedProfile);
            }
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_REFRESH_ID);
        KillTimer(hWnd, 2);  // Single-click delayed action timer
        _RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// _ImportProfile
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// _EncryptProfileViaMgr
// Starts the WireGuardManager service briefly so it encrypts a plain .conf
// file into .conf.dpapi, then stops and disables it again.
// pwszConfPath    – full path to the .conf file that was copied
// pwszProfileName – profile name without extension (used for log messages)
// Returns true if the .conf.dpapi file was produced within the timeout.
// ---------------------------------------------------------------------------
bool WireGuardTrayApp::_EncryptProfileViaMgr(PCWSTR pwszConfPath,
                                              PCWSTR pwszProfileName)
{
    // Build the expected .conf.dpapi path
    WCHAR wszDpapi[MAX_PATH_WGCP] = {};
    StringCchPrintfW(wszDpapi, MAX_PATH_WGCP, L"%s.dpapi", pwszConfPath);

    // Open the SCM and the WireGuardManager service
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr,
                                     SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
    if (!hSCM)
    {
        LOG_WARN(L"EncryptViaMgr: OpenSCManager failed");
        return false;
    }

    SC_HANDLE hSvc = OpenServiceW(hSCM, L"WireGuardManager",
                                   SERVICE_START | SERVICE_STOP |
                                   SERVICE_QUERY_STATUS | SERVICE_CHANGE_CONFIG);
    if (!hSvc)
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"EncryptViaMgr: OpenService failed err=%lu", GetLastError());
        LOG_WARN(e);
        CloseServiceHandle(hSCM);
        return false;
    }

    // Re-enable the service temporarily (it was set to DISABLED)
    if (!ChangeServiceConfigW(hSvc, SERVICE_NO_CHANGE, SERVICE_DEMAND_START,
                               SERVICE_NO_CHANGE, nullptr, nullptr, nullptr,
                               nullptr, nullptr, nullptr, nullptr))
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"EncryptViaMgr: ChangeConfig(DEMAND_START) failed err=%lu", GetLastError());
        LOG_WARN(e);
        CloseServiceHandle(hSvc);
        CloseServiceHandle(hSCM);
        return false;
    }
    LOG_DEBUG(L"EncryptViaMgr: WireGuardManager re-enabled (DEMAND_START)");

    // Start the service
    if (!StartServiceW(hSvc, 0, nullptr))
    {
        DWORD dwErr = GetLastError();
        if (dwErr != ERROR_SERVICE_ALREADY_RUNNING)
        {
            WCHAR e[64] = {};
            StringCchPrintfW(e, 64, L"EncryptViaMgr: StartService failed err=%lu", dwErr);
            LOG_WARN(e);
            // Re-disable before returning
            ChangeServiceConfigW(hSvc, SERVICE_NO_CHANGE, SERVICE_DISABLED,
                                  SERVICE_NO_CHANGE, nullptr, nullptr, nullptr,
                                  nullptr, nullptr, nullptr, nullptr);
            CloseServiceHandle(hSvc);
            CloseServiceHandle(hSCM);
            return false;
        }
    }
    LOG_DEBUG(L"EncryptViaMgr: WireGuardManager started");

    // Poll for the .conf.dpapi file – the manager encrypts it shortly after start
    // Timeout: 10 seconds, poll every 250 ms
    bool bEncrypted = false;
    for (int i = 0; i < 40; i++)
    {
        Sleep(250);
        if (GetFileAttributesW(wszDpapi) != INVALID_FILE_ATTRIBUTES)
        {
            bEncrypted = true;
            WCHAR dbg[MAX_PATH_WGCP + 32] = {};
            StringCchPrintfW(dbg, ARRAYSIZE(dbg),
                L"EncryptViaMgr: .conf.dpapi appeared after %d ms", (i + 1) * 250);
            LOG_DEBUG(dbg);
            break;
        }
    }

    if (!bEncrypted)
        LOG_WARN(L"EncryptViaMgr: timeout – .conf.dpapi did not appear within 10 s");

    // Stop the service
    SERVICE_STATUS ss = {};
    if (!ControlService(hSvc, SERVICE_CONTROL_STOP, &ss))
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"EncryptViaMgr: ControlService(STOP) failed err=%lu", GetLastError());
        LOG_WARN(e);
    }
    else
    {
        // Wait up to 5 s for the service to actually stop
        for (int i = 0; i < 20; i++)
        {
            Sleep(250);
            QueryServiceStatus(hSvc, &ss);
            if (ss.dwCurrentState == SERVICE_STOPPED) break;
        }
        LOG_DEBUG(L"EncryptViaMgr: WireGuardManager stopped");
    }

    // Re-disable the service
    if (ChangeServiceConfigW(hSvc, SERVICE_NO_CHANGE, SERVICE_DISABLED,
                               SERVICE_NO_CHANGE, nullptr, nullptr, nullptr,
                               nullptr, nullptr, nullptr, nullptr))
        LOG_DEBUG(L"EncryptViaMgr: WireGuardManager disabled again");
    else
        LOG_WARN(L"EncryptViaMgr: could not re-disable WireGuardManager");

    CloseServiceHandle(hSvc);
    CloseServiceHandle(hSCM);
    return bEncrypted;
}

// ---------------------------------------------------------------------------
// _ImportProfile
// Opens a file picker, copies the .conf to the WireGuard config directory,
// triggers encryption via WireGuardManager, then reloads the profile list.
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
        sei.lpVerb       = L"runas";
        sei.lpFile       = L"cmd.exe";
        sei.lpParameters = wszCmd;
        sei.nShow        = SW_HIDE;
        sei.fMask        = SEE_MASK_NOCLOSEPROCESS;

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

    // Strip extension for profile name and log/UI messages
    WCHAR wszProfile[MAX_PATH_WGCP] = {};
    StringCchCopyW(wszProfile, MAX_PATH_WGCP, pwszFileName);
    WCHAR* pExt = wcsrchr(wszProfile, L'.');
    if (pExt) *pExt = L'\0';

    WCHAR wszLog[MAX_PATH_WGCP + 64] = {};
    StringCchPrintfW(wszLog, ARRAYSIZE(wszLog),
        L"Tray: Profile '%s' copied to config directory - starting encryption", wszProfile);
    LOG_DEBUG(wszLog);

    // --- NEW: Encrypt .conf -> .conf.dpapi via WireGuardManager ---
    // Show a brief "please wait" balloon so the user knows something is happening
    _ShowBalloon(
        T(L"Profil wird verschl\u00fcsselt...", L"Encrypting profile..."),
        T(L"Bitte warten, das Profil wird verarbeitet.",
          L"Please wait while the profile is being processed."),
        NIIF_INFO);

    bool bEncrypted = _EncryptProfileViaMgr(wszDest, wszProfile);

    if (bEncrypted)
    {
        // Remove the plain-text .conf – only the .conf.dpapi is needed
        if (!DeleteFileW(wszDest))
        {
            WCHAR e[64] = {};
            StringCchPrintfW(e, 64, L"Tray: Could not delete plain .conf err=%lu", GetLastError());
            LOG_WARN(e);
            // Non-fatal: WireGuard will still use the .conf.dpapi
        }
        else
            LOG_DEBUG(L"Tray: Plain .conf removed after encryption");
    }
    else
    {
        // Encryption failed – inform the user; leave the .conf in place so
        // the administrator can investigate or retry manually.
        MessageBoxW(_hWnd,
            T(L"Das Profil wurde kopiert, konnte aber nicht automatisch\n"
              L"verschl\u00fcsselt werden. Bitte starten Sie den\n"
              L"WireGuardManager-Dienst einmalig manuell, um die\n"
              L"Verschl\u00fcsselung abzuschlie\u00dfen.",
              L"The profile was copied but could not be encrypted automatically.\n"
              L"Please start the WireGuardManager service once manually\n"
              L"to complete the encryption."),
            T(L"Verschl\u00fcsselung fehlgeschlagen", L"Encryption failed"),
            MB_ICONWARNING | MB_OK);
    }

    // Reload profiles so the newly imported one appears in the menu immediately
    _LoadProfiles();
    _RefreshStatus();
    _UpdateTrayIcon();

    if (bEncrypted)
    {
        WCHAR wszMsg[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(wszMsg, ARRAYSIZE(wszMsg),
            T(L"Profil \u201e%s\u201c wurde erfolgreich importiert.",
              L"Profile \u201c%s\u201d was imported successfully."),
            wszProfile);
        _ShowBalloon(
            T(L"Profil importiert", L"Profile imported"),
            wszMsg, NIIF_INFO);
    }
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

// ---------------------------------------------------------------------------
// _DisableWireGuardManager
// Disables the WireGuard Manager service which auto-spawns wireguard.exe UI.
// The tunnel services (WireGuardTunnel$*) are not affected.
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_DisableWireGuardManager()
{
    SC_HANDLE hSCM = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hSCM) return;

    SC_HANDLE hSvc = OpenServiceW(hSCM, L"WireGuardManager",
                                   SERVICE_CHANGE_CONFIG | SERVICE_STOP |
                                   SERVICE_QUERY_STATUS);
    if (hSvc)
    {
        // Stop the service if running
        SERVICE_STATUS ss = {};
        QueryServiceStatus(hSvc, &ss);
        if (ss.dwCurrentState == SERVICE_RUNNING)
        {
            ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
            LOG_DEBUG(L"WireGuardManager: service stopped");
        }
        // Disable: set start type to DISABLED
        if (ChangeServiceConfigW(hSvc, SERVICE_NO_CHANGE, SERVICE_DISABLED,
                                  SERVICE_NO_CHANGE, nullptr, nullptr, nullptr,
                                  nullptr, nullptr, nullptr, nullptr))
            LOG_DEBUG(L"WireGuardManager: service disabled");
        else
            LOG_WARN(L"WireGuardManager: could not disable service");
        CloseServiceHandle(hSvc);
    }
    CloseServiceHandle(hSCM);
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
                WCHAR d[96] = {};
                StringCchPrintfW(d, ARRAYSIZE(d),
                    L"Tray: WireGuard UI process detected (PID %lu) - terminating to prevent conflict", dwPid);
                LOG_WARN(d);

                // Step 1: Send WM_CLOSE to all windows so the process can
                //         clean up its tray icon via Shell_NotifyIcon(NIM_DELETE)
                struct CloseData { DWORD pid; };
                CloseData cd = { dwPid };
                EnumWindows([](HWND hW, LPARAM lp) -> BOOL {
                    DWORD pid = 0;
                    GetWindowThreadProcessId(hW, &pid);
                    if (pid == reinterpret_cast<CloseData*>(lp)->pid)
                    {
                        ShowWindow(hW, SW_HIDE);       // hide immediately
                        PostMessageW(hW, WM_CLOSE, 0, 0); // ask to close
                    }
                    return TRUE;
                }, reinterpret_cast<LPARAM>(&cd));

                // Step 2: Wait up to 1 second for graceful exit
                DWORD dwWait = WaitForSingleObject(hProc, 1000);

                // Step 3: Force-terminate if still running
                if (dwWait != WAIT_OBJECT_0)
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
            WCHAR dIns[320] = {};
            StringCchPrintfW(dIns, ARRAYSIZE(dIns), L"SC Watcher: Card inserted in reader '%s'", wszReader);
            LOG_DEBUG(dIns);

            // Seriennummer via ykman ermitteln und cachen (laeuft im Hintergrund-
            // Thread – kein UI-Freeze beim Menueöffnen).
            // _wszYkSerial bleibt erhalten solange die Karte steckt.
            if (pApp->_wszYkSerial[0] == L'\0')
            {
                WCHAR wszTmp[MAX_PATH] = {};
                GetTempPathW(MAX_PATH, wszTmp);
                WCHAR wszTmpF[MAX_PATH] = {};
                StringCchPrintfW(wszTmpF, MAX_PATH,
                    L"%swgcp_yk_%lu.txt", wszTmp, GetCurrentProcessId());

                WCHAR wszCmd[256] = {};
                StringCchPrintfW(wszCmd, 256,
                    L"cmd.exe /C ykman info > \"%s\" 2>NUL", wszTmpF);
                STARTUPINFOW si = { sizeof(si) };
                PROCESS_INFORMATION pi = {};
                if (CreateProcessW(nullptr, wszCmd, nullptr, nullptr, FALSE,
                    CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
                {
                    WaitForSingleObject(pi.hProcess, 3000);
                    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
                    HANDLE hF = CreateFileW(wszTmpF, GENERIC_READ, FILE_SHARE_READ,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (hF != INVALID_HANDLE_VALUE)
                    {
                        char buf[512] = {}; DWORD dw = 0;
                        ReadFile(hF, buf, sizeof(buf) - 1, &dw, nullptr);
                        CloseHandle(hF);
                        DeleteFileW(wszTmpF);
                        char* p = strstr(buf, "Serial number:");
                        if (p)
                        {
                            p += 14; while (*p == ' ') p++;
                            char szSer[16] = {}; int j = 0;
                            while (*p && *p != '\r' && *p != '\n' && j < 15)
                                szSer[j++] = *p++;
                            MultiByteToWideChar(CP_ACP, 0, szSer, -1,
                                pApp->_wszYkSerial, ARRAYSIZE(pApp->_wszYkSerial));
                            WCHAR dSer[64] = {};
                            StringCchPrintfW(dSer, 64, L"SC Watcher: YubiKey serial cached: %s",
                                pApp->_wszYkSerial);
                            LOG_DEBUG(dSer);
                        }
                    }
                    else DeleteFileW(wszTmpF);
                }
            }

            if (pApp->_scConfig.bConnectOnInsert && !pApp->_bConnected
                && pApp->_nProfiles > 0)
            {
                WCHAR dAC[MAX_PATH_WGCP + 64] = {};
                StringCchPrintfW(dAC, ARRAYSIZE(dAC),
                    L"SC Watcher: Auto-connect triggered for profile '%s'",
                    pApp->_rgProfiles[pApp->_nSelectedProfile]);
                LOG_DEBUG(dAC);
                PostMessageW(pApp->_hWnd, WM_COMMAND,
                             MAKEWPARAM(IDM_CONNECT, 0), 0);
            }
        }
        else if (!bCardPresent && bCardWasPresentLastTick)
        {
            // Card just removed
            WCHAR dRem[320] = {};
            StringCchPrintfW(dRem, ARRAYSIZE(dRem), L"SC Watcher: Card removed from reader '%s'", wszReader);
            LOG_DEBUG(dRem);

            if (pApp->_scConfig.bDisconnectOnRemove && pApp->_bConnected)
            {
                LOG_DEBUG(L"SC Watcher: Auto-disconnect triggered due to card removal");
                PostMessageW(pApp->_hWnd, WM_COMMAND,
                             MAKEWPARAM(IDM_DISCONNECT, 0), 0);
            }
            // Serial-Cache leeren: naechste Karte koennte eine andere sein
            ZeroMemory(pApp->_wszYkSerial, sizeof(pApp->_wszYkSerial));
            ZeroMemory(wszReader, sizeof(wszReader));
        }

        bCardWasPresentLastTick = bCardPresent;
    }

    LOG_DEBUG(L"SC Watcher: Thread stopped");
    return 0;
}

// ---------------------------------------------------------------------------
// Corporate network watcher
// Monitors NLA (Network Location Awareness) for domain-authenticated networks.
// When the machine is detected on the corporate network, the VPN tunnel is
// automatically disconnected (no VPN needed inside the office).
// ---------------------------------------------------------------------------

void WireGuardTrayApp::_StartNetworkWatcher()
{
    _hNetWatchStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!_hNetWatchStop) { LOG_WARN(L"NetWatch: CreateEvent failed"); return; }

    _hNetWatchThread = CreateThread(nullptr, 0, _NetworkWatchThread, this, 0, nullptr);
    if (!_hNetWatchThread)
    {
        CloseHandle(_hNetWatchStop);
        _hNetWatchStop = nullptr;
        LOG_WARN(L"NetWatch: CreateThread failed");
    }
    else LOG_DEBUG(L"Tray: Network watcher started");
}

void WireGuardTrayApp::_StopNetworkWatcher()
{
    if (_hNetWatchStop)  SetEvent(_hNetWatchStop);
    if (_hNetWatchThread)
    {
        WaitForSingleObject(_hNetWatchThread, 5000);
        CloseHandle(_hNetWatchThread);
        _hNetWatchThread = nullptr;
    }
    if (_hNetWatchStop) { CloseHandle(_hNetWatchStop); _hNetWatchStop = nullptr; }
}

DWORD WINAPI WireGuardTrayApp::_NetworkWatchThread(LPVOID lpParam)
{
    WireGuardTrayApp* pApp = reinterpret_cast<WireGuardTrayApp*>(lpParam);
    LOG_DEBUG(L"NetWatch: Thread running");

    // Initialer Corporate-Network-Check vor der ersten Schleifenrunde.
    // Ohne das würde beim Start bOnCorp=false sein und ein bereits aktiver
    // Tunnel sofort einen Handshake-Timeout auslösen, obwohl der PC im
    // Firmennetz ist (z.B. VM direkt nach Login mit aktivem Tunnel).
    bool bWasOnCorp = WGCPIsOnCorporateNetwork();
    if (bWasOnCorp)
        LOG_DEBUG(L"NetWatch: Initial check - corporate network detected at startup");
    else
        LOG_DEBUG(L"NetWatch: Initial check - not on corporate network at startup");

    int nHandshakeFailCount = 0;

    while (WaitForSingleObject(pApp->_hNetWatchStop, 10000) == WAIT_TIMEOUT)
    {
        bool bOnCorp = WGCPIsOnCorporateNetwork();

        // Handshake-WARN nur loggen wenn nicht im Firmennetz (dort kein Handshake erwartet)
        if (pApp->_bConnected && pApp->_dwHandshakeTimeoutSec > 0
            && pApp->_nProfiles > 0
            && !bOnCorp)
        {
            LONGLONG llAge = WGGetLastHandshakeSec(
                pApp->_wszWgExePath,
                pApp->_rgProfiles[pApp->_nSelectedProfile]);

            if (llAge == -1)
            {
                nHandshakeFailCount = 0;
            }
            else if (llAge > static_cast<LONGLONG>(pApp->_dwHandshakeTimeoutSec))
            {
                nHandshakeFailCount++;
                WCHAR d[160] = {};
                StringCchPrintfW(d, ARRAYSIZE(d),
                    L"Handshake: last handshake %lld s ago (limit %lu s), consecutive=%d",
                    llAge, pApp->_dwHandshakeTimeoutSec, nHandshakeFailCount);
                LOG_WARN(d);

                if (nHandshakeFailCount >= 2)
                {
                    WCHAR dC[160] = {};
                    StringCchPrintfW(dC, ARRAYSIZE(dC),
                        L"Handshake timeout confirmed (%d checks) - disconnecting tunnel '%s'",
                        nHandshakeFailCount,
                        pApp->_rgProfiles[pApp->_nSelectedProfile]);
                    LOG_CRIT(dC);

                    nHandshakeFailCount = 0;
                    PostMessageW(pApp->_hWnd, WM_COMMAND,
                                 MAKEWPARAM(IDM_DISCONNECT, 0), 0);

                    pApp->_ShowBalloon(L"WireGuard VPN",
                        T(L"Verbindung getrennt: Kein Handshake.",
                          L"Disconnected: Handshake timeout."),
                        NIIF_WARNING);
                }
            }
            else
            {
                nHandshakeFailCount = 0;
            }
        }
        else if (bOnCorp)
        {
            nHandshakeFailCount = 0;
        }

        // Corporate-Network-Disconnect: bei JEDEM Check trennen wenn verbunden.
        // Nicht nur beim Übergang – sonst greift es nicht wenn der Nutzer
        // manuell verbindet während er schon im Firmennetz ist.
        if (bOnCorp && pApp->_bConnected && pApp->_nProfiles > 0)
        {
            LOG_DEBUG(L"NetWatch: Corporate network active and tunnel connected - disconnecting");
            PostMessageW(pApp->_hWnd, WM_COMMAND,
                         MAKEWPARAM(IDM_DISCONNECT, 0), 0);
            pApp->_ShowBalloon(L"WireGuard VPN",
                T(L"Firmennetz erkannt \u2013 VPN getrennt.",
                  L"Corporate network detected \u2013 VPN disconnected."),
                NIIF_INFO, 8000);
        }

        // Balloon nur beim Übergang false→true ohne aktive Verbindung
        if (bOnCorp && !bWasOnCorp && !pApp->_bConnected)
        {
            LOG_DEBUG(L"NetWatch: Corporate network detected (transition, VPN not active)");
            pApp->_ShowBalloon(L"WireGuard VPN",
                T(L"Firmennetz erkannt \u2013 kein VPN erforderlich.",
                  L"Corporate network detected \u2013 no VPN required."),
                NIIF_INFO, 8000);
        }
        else if (!bOnCorp && bWasOnCorp)
        {
            LOG_DEBUG(L"NetWatch: Left corporate network - VPN is now available again");
        }

        bWasOnCorp = bOnCorp;
    }

    LOG_DEBUG(L"NetWatch: Thread stopped");
    return 0;
}

// ---------------------------------------------------------------------------
// _DeleteProfile – delete currently selected .conf.dpapi profile
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// _DeleteProfileAt – delete the profile at the given index after confirmation.
// Called from the submenu "Delete" entry (IDM_PROFILE_DELETE_BASE + i).
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// _EditProfile  –  Decrypt, edit and re-encrypt a WireGuard profile
// ---------------------------------------------------------------------------
// The .conf.dpapi file is a standard Windows DPAPI blob encrypted with
// CRYPTPROTECT_LOCAL_MACHINE by the WireGuardManager (SYSTEM) service.
// Running as Administrator we can decrypt and re-encrypt on the same machine.
// ---------------------------------------------------------------------------

// Dialog proc for the profile editor (file-scope, before _EditProfile)
struct WGEditDlgData
{
    WireGuardTrayApp* pApp;
    WCHAR  wszProfileName[MAX_PATH_WGCP];
    WCHAR* pwszConf;     // plaintext config buffer (caller allocates, dialog reads)
    WCHAR* pwszResult;   // edited result (dialog allocates with new[], caller frees)
    bool   bSaved;
};

static INT_PTR CALLBACK _EditDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static WGEditDlgData* s_pData = nullptr;

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        s_pData = reinterpret_cast<WGEditDlgData*>(lParam);

        // Title
        WCHAR wszTitle[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(wszTitle, ARRAYSIZE(wszTitle),
            T(L"Profil bearbeiten \u2013 %s", L"Edit profile \u2013 %s"),
            s_pData->wszProfileName);
        SetWindowTextW(hDlg, wszTitle);

        // Populate the edit control
        HWND hEdit = GetDlgItem(hDlg, IDC_WGEDIT_TEXT);
        if (hEdit && s_pData->pwszConf)
            SetWindowTextW(hEdit, s_pData->pwszConf);

        // Set monospace font
        HFONT hFont = CreateFontW(
            -MulDiv(10, GetDeviceCaps(GetDC(nullptr), LOGPIXELSY), 72),
            0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
            L"Consolas");
        if (hFont && hEdit)
            SendMessageW(hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && s_pData)
        {
            HWND hEdit = GetDlgItem(hDlg, IDC_WGEDIT_TEXT);
            if (hEdit)
            {
                int cch = GetWindowTextLengthW(hEdit) + 1;
                s_pData->pwszResult = new(std::nothrow) WCHAR[cch];
                if (s_pData->pwszResult)
                {
                    GetWindowTextW(hEdit, s_pData->pwszResult, cch);
                    s_pData->bSaved = true;
                }
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL)
        {
            s_pData->bSaved = false;
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        if (s_pData) s_pData->bSaved = false;
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// _ImpersonateAsSystem  –  impersonate SYSTEM via winlogon.exe token
//
// WireGuard encrypts .conf.dpapi under SYSTEM's DPAPI context (no
// CRYPTPROTECT_LOCAL_MACHINE flag).  An Administrator process must
// impersonate SYSTEM to decrypt/encrypt those blobs.
// Call RevertToSelf() after the DPAPI operation.
// ---------------------------------------------------------------------------
static bool _ImpersonateAsSystem()
{
    // Enable SeDebugPrivilege so we can open protected SYSTEM processes
    HANDLE hSelf = nullptr;
    if (OpenProcessToken(GetCurrentProcess(),
                         TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hSelf))
    {
        TOKEN_PRIVILEGES tp   = {};
        tp.PrivilegeCount     = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME,
                              &tp.Privileges[0].Luid);
        AdjustTokenPrivileges(hSelf, FALSE, &tp, sizeof(tp),
                              nullptr, nullptr);
        CloseHandle(hSelf);
    }

    // Find winlogon.exe – always runs as SYSTEM
    DWORD dwWinlogonPid = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W pe = {};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe))
        {
            do {
                if (_wcsicmp(pe.szExeFile, L"winlogon.exe") == 0)
                { dwWinlogonPid = pe.th32ProcessID; break; }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
    if (!dwWinlogonPid) return false;

    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwWinlogonPid);
    if (!hProc) return false;

    HANDLE hSysToken = nullptr;
    bool bOk = false;
    if (OpenProcessToken(hProc, TOKEN_DUPLICATE, &hSysToken))
    {
        HANDLE hImpToken = nullptr;
        if (DuplicateTokenEx(hSysToken,
                             TOKEN_IMPERSONATE | TOKEN_QUERY,
                             nullptr, SecurityImpersonation,
                             TokenImpersonation, &hImpToken))
        {
            bOk = SetThreadToken(nullptr, hImpToken) != FALSE;
            CloseHandle(hImpToken);
        }
        CloseHandle(hSysToken);
    }
    CloseHandle(hProc);
    return bOk;
}

// ---------------------------------------------------------------------------
// Named pipe helpers: proxy file I/O through WireGuardShutdownHelper (SYSTEM)
// so we can read/write SYSTEM-only .conf.dpapi files from the user-context tray.
// ---------------------------------------------------------------------------
#define WGCP_PIPE_NAME  L"\\\\.\\pipe\\WireGuardCPHelperSvc"

static bool _PipeExact(HANDLE h, void* p, DWORD cb, bool bRead)
{
    DWORD done = 0, chunk = 0;
    while (done < cb)
    {
        BOOL ok = bRead
            ? ReadFile(h, (BYTE*)p + done, cb - done, &chunk, nullptr)
            : WriteFile(h, (BYTE*)p + done, cb - done, &chunk, nullptr);
        if (!ok || chunk == 0) return false;
        done += chunk;
    }
    return true;
}

// Send a pipe request and receive a response.
// op 3 = READ_DECRYPTED:  service reads + DPAPI-decrypts → returns plaintext bytes
// op 4 = WRITE_ENCRYPTED: tray sends plaintext → service DPAPI-encrypts + writes file
static bool _SvcPipeOp(DWORD op, PCWSTR wszPath,
                        const BYTE* pSendData, DWORD dwSendSize,
                        BYTE** ppRecvData,      DWORD* pdwRecvSize)
{
    HANDLE hPipe = CreateFileW(WGCP_PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
                               0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hPipe == INVALID_HANDLE_VALUE) return false;

    DWORD pathBytes = static_cast<DWORD>(wcslen(wszPath) * sizeof(WCHAR));
    bool ok = _PipeExact(hPipe, &op,        4, false) &&
              _PipeExact(hPipe, &pathBytes, 4, false) &&
              _PipeExact(hPipe, &dwSendSize,4, false) &&
              _PipeExact(hPipe, (void*)wszPath, pathBytes, false);
    if (ok && pSendData && dwSendSize > 0)
        ok = _PipeExact(hPipe, (void*)pSendData, dwSendSize, false);
    if (!ok) { CloseHandle(hPipe); return false; }

    DWORD result = 0, recvSize = 0;
    ok = _PipeExact(hPipe, &result,   4, true) &&
         _PipeExact(hPipe, &recvSize, 4, true);
    if (!ok || result != 0) { CloseHandle(hPipe); SetLastError(result); return false; }

    if (ppRecvData && recvSize > 0)
    {
        BYTE* buf = new(std::nothrow) BYTE[recvSize];
        ok = buf && _PipeExact(hPipe, buf, recvSize, true);
        CloseHandle(hPipe);
        if (!ok) { delete[] buf; return false; }
        *ppRecvData  = buf;
        *pdwRecvSize = recvSize;
    }
    else
    {
        CloseHandle(hPipe);
        if (ppRecvData)  { *ppRecvData  = nullptr; }
        if (pdwRecvSize) { *pdwRecvSize = 0; }
    }
    return true;
}

// Read + decrypt a .conf.dpapi via the SYSTEM service pipe (op 3).
// Returns true; caller must delete[] *ppPlaintext.
static bool _SvcReadDecrypted(PCWSTR wszPath, BYTE** ppPlaintext, DWORD* pdwSize)
{
    return _SvcPipeOp(3, wszPath, nullptr, 0, ppPlaintext, pdwSize);
}

// Write plaintext bytes as a .conf file via the SYSTEM service pipe (op 5).
static bool _SvcWriteConf(PCWSTR wszPath, const BYTE* pData, DWORD dwSize)
{
    return _SvcPipeOp(5, wszPath, pData, dwSize, nullptr, nullptr);
}

// Delete a file (.conf or .conf.dpapi) via the SYSTEM service pipe (op 6).
static bool _SvcDeleteFile(PCWSTR wszPath)
{
    return _SvcPipeOp(6, wszPath, nullptr, 0, nullptr, nullptr);
}

void WireGuardTrayApp::_EditProfile(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= _nProfiles) return;

    PCWSTR pwszProfile = _rgProfiles[profileIndex];

    // Build full path to .conf.dpapi
    WCHAR wszConfigDir[MAX_PATH_WGCP] = {};
    WGGetConfigDir(wszConfigDir, MAX_PATH_WGCP);
    WCHAR wszDpapiPath[MAX_PATH_WGCP] = {};
    StringCchPrintfW(wszDpapiPath, MAX_PATH_WGCP, L"%s%s%s",
                     wszConfigDir, pwszProfile, WG_CONFIG_EXT);

    // --- Read + decrypt via ShutdownHelper service (op 3: READ_DECRYPTED) ---
    // Config files are SYSTEM-only ACL; DPAPI LOCAL_MACHINE blobs also require
    // elevated/SYSTEM context to decrypt. Both are handled by the service pipe.
    BYTE* pbPlain = nullptr;
    DWORD dwPlainLen = 0;
    if (!_SvcReadDecrypted(wszDpapiPath, &pbPlain, &dwPlainLen))
    {
        DWORD dwErr = GetLastError();
        WCHAR wszErrMsg[MAX_PATH_WGCP + 256] = {};
        StringCchPrintfW(wszErrMsg, ARRAYSIZE(wszErrMsg),
            T(L"Konfigurationsdatei konnte nicht ge\u00F6ffnet werden.\r\n"
              L"Pfad: %s\r\nFehler: %lu\r\n\r\n"
              L"Stellen Sie sicher, dass der WireGuard Shutdown Helper-Dienst l\u00E4uft.",
              L"Configuration file could not be opened.\r\n"
              L"Path: %s\r\nError: %lu\r\n\r\n"
              L"Make sure the WireGuard Shutdown Helper service is running."),
            wszDpapiPath, dwErr);
        MessageBoxW(_hWnd, wszErrMsg,
            T(L"Fehler", L"Error"), MB_ICONERROR | MB_OK | MB_SETFOREGROUND);
        return;
    }

    // Convert decrypted UTF-8 bytes to wide string for the editor
    int cchWide = MultiByteToWideChar(CP_UTF8, 0,
                                      reinterpret_cast<LPCSTR>(pbPlain),
                                      static_cast<int>(dwPlainLen),
                                      nullptr, 0);
    WCHAR* pwszConf = new(std::nothrow) WCHAR[cchWide + 1];
    if (!pwszConf) { delete[] pbPlain; return; }
    MultiByteToWideChar(CP_UTF8, 0,
                        reinterpret_cast<LPCSTR>(pbPlain),
                        static_cast<int>(dwPlainLen),
                        pwszConf, cchWide);
    pwszConf[cchWide] = L'\0';
    delete[] pbPlain;

    // Normalise line endings to CRLF for the EDIT control.
    // 1) Strip bare \r (collapse \r\n -> \n, leave bare \n as-is)
    {
        WCHAR* pSrc = pwszConf;
        WCHAR* pDst = pwszConf;
        while (*pSrc)
        {
            if (*pSrc == L'\r') { pSrc++; continue; }  // strip \r
            *pDst++ = *pSrc++;
        }
        *pDst = L'\0';
    }
    // 2) Count \n to allocate new buffer with \r\n
    int cchLF = 0;
    for (WCHAR* p = pwszConf; *p; p++) if (*p == L'\n') cchLF++;
    int cchSrc = static_cast<int>(wcslen(pwszConf));
    WCHAR* pwszCRLF = new(std::nothrow) WCHAR[cchSrc + cchLF + 1];
    if (pwszCRLF)
    {
        WCHAR* pSrc = pwszConf;
        WCHAR* pDst = pwszCRLF;
        while (*pSrc)
        {
            if (*pSrc == L'\n') { *pDst++ = L'\r'; }
            *pDst++ = *pSrc++;
        }
        *pDst = L'\0';
        delete[] pwszConf;
        pwszConf = pwszCRLF;
    }

    // --- Show editor dialog (programmatic, no .rc template) ---
    // Build dialog template in memory
    // Layout: full-client multiline EDIT + OK/Cancel buttons at bottom
    bool bWasConnected = (_bConnected && profileIndex == _nSelectedProfile);

    if (bWasConnected)
    {
        int nWarn = MessageBoxW(_hWnd,
            T(L"Das Profil ist gerade verbunden.\r\n"
              L"Zum Bearbeiten muss die Verbindung getrennt werden.\r\n\r\n"
              L"Jetzt trennen und Profil \u00F6ffnen?",
              L"The profile is currently connected.\r\n"
              L"The connection must be disconnected to edit.\r\n\r\n"
              L"Disconnect now and open editor?"),
            T(L"Profil bearbeiten", L"Edit profile"),
            MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND);
        if (nWarn == IDNO) { delete[] pwszConf; return; }
        _Disconnect();
    }

    WGEditDlgData dlgData = {};
    dlgData.pApp    = this;
    StringCchCopyW(dlgData.wszProfileName, MAX_PATH_WGCP, pwszProfile);
    dlgData.pwszConf   = pwszConf;
    dlgData.pwszResult = nullptr;
    dlgData.bSaved     = false;

    // Build in-memory DLGTEMPLATE (no .rc needed)
    // Dialog: 500x380 DLU, resizable look, has IDC_WGEDIT_TEXT (edit) + OK + Cancel
    struct alignas(DWORD) DlgTmpl {
        DLGTEMPLATE hdr;
        WORD menu, cls, title;
    };

    // Use CreateDialogIndirectParam approach: build template manually
    // Simpler: use a plain window with CreateWindowEx
    //
    // We'll use DialogBoxIndirectParam with a hand-crafted template.
    // Template memory layout:
    //   DLGTEMPLATE  (18 bytes)
    //   menu  (WORD = 0, no menu)
    //   class (WORD = 0, default dialog class)
    //   title (WCHAR[] null-terminated)
    //   [items follow]
    //
    // But building the full item array is complex.
    // Use a simpler approach: RegisterClass + CreateWindowEx (modal via EnableWindow).

    WNDCLASSEXW wc = {};
    wc.cbSize       = sizeof(wc);
    wc.lpfnWndProc  = [](HWND hw, UINT m, WPARAM wp, LPARAM lp) -> LRESULT {
        if (m == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            SetWindowLongPtrW(hw, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return DefWindowProcW(hw, m, wp, lp);
        }
        auto* pd = reinterpret_cast<WGEditDlgData*>(GetWindowLongPtrW(hw, GWLP_USERDATA));

        if (m == WM_CREATE)
        {
            RECT rc; GetClientRect(hw, &rc);
            int btnH = 30, btnW = 90, pad = 8;
            int editH = rc.bottom - btnH - pad*3;

            HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL|
                ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL|ES_WANTRETURN,
                pad, pad, rc.right-pad*2, editH,
                hw, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_WGEDIT_TEXT)), nullptr, nullptr);

            HFONT hFont = CreateFontW(
                -MulDiv(10, GetDeviceCaps(GetDC(nullptr), LOGPIXELSY), 72),
                0,0,0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, FIXED_PITCH|FF_MODERN, L"Consolas");
            if (hFont) SendMessageW(hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            if (pd && pd->pwszConf) SetWindowTextW(hEdit, pd->pwszConf);

            int btnY = editH + pad*2;
            int btnX = rc.right - (btnW+pad)*2;
            CreateWindowExW(0, L"BUTTON",
                T(L"Speichern", L"Save"),
                WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,
                btnX, btnY, btnW, btnH,
                hw, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDOK)), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON",
                T(L"Abbrechen", L"Cancel"),
                WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
                btnX+btnW+pad, btnY, btnW, btnH,
                hw, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDCANCEL)), nullptr, nullptr);
            return 0;
        }
        if (m == WM_SIZE)
        {
            int btnH = 30, btnW = 90, pad = 8;
            int W = LOWORD(lp), H = HIWORD(lp);
            int editH = H - btnH - pad*3;
            HWND hEdit = GetDlgItem(hw, IDC_WGEDIT_TEXT);
            if (hEdit) SetWindowPos(hEdit, nullptr, pad, pad, W-pad*2, editH, SWP_NOZORDER);
            int btnY = editH + pad*2;
            int btnX = W - (btnW+pad)*2;
            HWND hOK  = GetDlgItem(hw, IDOK);
            HWND hCan = GetDlgItem(hw, IDCANCEL);
            if (hOK)  SetWindowPos(hOK,  nullptr, btnX, btnY, btnW, btnH, SWP_NOZORDER);
            if (hCan) SetWindowPos(hCan, nullptr, btnX+btnW+pad, btnY, btnW, btnH, SWP_NOZORDER);
            return 0;
        }
        if (m == WM_COMMAND)
        {
            WORD id = LOWORD(wp);
            if (id == IDOK && pd)
            {
                HWND hEdit = GetDlgItem(hw, IDC_WGEDIT_TEXT);
                if (hEdit) {
                    int cch = GetWindowTextLengthW(hEdit) + 1;
                    pd->pwszResult = new(std::nothrow) WCHAR[cch];
                    if (pd->pwszResult) { GetWindowTextW(hEdit, pd->pwszResult, cch); pd->bSaved = true; }
                }
                EnableWindow(reinterpret_cast<HWND>(GetWindowLongPtrW(hw, GWLP_HWNDPARENT)), TRUE);
                DestroyWindow(hw);
            }
            if (id == IDCANCEL)
            {
                EnableWindow(reinterpret_cast<HWND>(GetWindowLongPtrW(hw, GWLP_HWNDPARENT)), TRUE);
                DestroyWindow(hw);
            }
            return 0;
        }
        if (m == WM_KEYDOWN && wp == VK_ESCAPE) {
            EnableWindow(reinterpret_cast<HWND>(GetWindowLongPtrW(hw, GWLP_HWNDPARENT)), TRUE);
            DestroyWindow(hw);
            return 0;
        }
        if (m == WM_DESTROY) { return 0; }
        return DefWindowProcW(hw, m, wp, lp);
    };
    wc.hInstance    = _hInst;
    wc.hCursor      = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"WGCPProfileEditor";
    RegisterClassExW(&wc);  // may fail if already registered – that's fine

    // Compute centered position
    int dlgW = 640, dlgH = 480;
    int scW  = GetSystemMetrics(SM_CXSCREEN);
    int scH  = GetSystemMetrics(SM_CYSCREEN);
    int posX = (scW - dlgW) / 2;
    int posY = (scH - dlgH) / 2;

    WCHAR wszEdTitle[MAX_PATH_WGCP + 64] = {};
    StringCchPrintfW(wszEdTitle, ARRAYSIZE(wszEdTitle),
        T(L"Profil bearbeiten \u2013 %s", L"Edit profile \u2013 %s"),
        pwszProfile);

    EnableWindow(_hWnd, FALSE);  // modal behaviour
    HWND hEditor = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"WGCPProfileEditor", wszEdTitle,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        posX, posY, dlgW, dlgH,
        _hWnd, nullptr, _hInst, &dlgData);

    if (!hEditor) { EnableWindow(_hWnd, TRUE); delete[] pwszConf; return; }

    // Modal message loop – runs until editor window is destroyed.
    // Re-posts WM_QUIT if received so the main tray loop can exit cleanly.
    MSG edMsg = {};
    while (IsWindow(hEditor) && GetMessageW(&edMsg, nullptr, 0, 0) > 0)
    {
        if (!IsWindow(hEditor)) break;
        if (IsDialogMessageW(hEditor, &edMsg)) continue;
        TranslateMessage(&edMsg);
        DispatchMessageW(&edMsg);
    }
    if (edMsg.message == WM_QUIT)
        PostQuitMessage(static_cast<int>(edMsg.wParam));

    delete[] pwszConf;

    if (!dlgData.bSaved || !dlgData.pwszResult)
    {
        delete[] dlgData.pwszResult;
        if (bWasConnected) _Connect(_nSelectedProfile);  // restore connection
        return;
    }

    // --- Convert edited wide string back to UTF-8 (no BOM) ---
    int cbUtf8 = WideCharToMultiByte(CP_UTF8, 0, dlgData.pwszResult, -1,
                                     nullptr, 0, nullptr, nullptr);
    char* pUtf8 = new(std::nothrow) char[cbUtf8];
    if (!pUtf8) { delete[] dlgData.pwszResult; return; }
    WideCharToMultiByte(CP_UTF8, 0, dlgData.pwszResult, -1, pUtf8, cbUtf8, nullptr, nullptr);
    delete[] dlgData.pwszResult;
    cbUtf8--;  // exclude null terminator

    // --- Save via WireGuard's own import mechanism ---
    // Strategy: write plaintext .conf → delete .conf.dpapi → let WireGuard
    // encrypt it via /installtunnelservice → immediately /uninstalltunnelservice
    // → clean up .conf. WireGuard's encryption is guaranteed compatible.
    WCHAR wszConfPath[MAX_PATH_WGCP] = {};
    StringCchPrintfW(wszConfPath, MAX_PATH_WGCP, L"%s%s.conf", wszConfigDir, pwszProfile);

    // 1. Write plaintext .conf (via service pipe, SYSTEM writes to the protected dir)
    bool bConfOk = _SvcWriteConf(wszConfPath,
                                  reinterpret_cast<BYTE*>(pUtf8),
                                  static_cast<DWORD>(cbUtf8));
    delete[] pUtf8;
    if (!bConfOk)
    {
        DWORD e = GetLastError();
        WCHAR wszErr[128] = {};
        StringCchPrintfW(wszErr, ARRAYSIZE(wszErr),
            T(L"Speichern fehlgeschlagen – .conf konnte nicht geschrieben werden (Fehler %lu).",
              L"Save failed – .conf could not be written (error %lu)."), e);
        MessageBoxW(_hWnd, wszErr, T(L"Fehler", L"Error"),
                    MB_ICONERROR | MB_OK | MB_SETFOREGROUND);
        if (bWasConnected) _Connect(_nSelectedProfile);
        return;
    }

    // 2. Delete old .conf.dpapi (service pipe)
    _SvcDeleteFile(wszDpapiPath);  // ignore error – may already be gone

    // 3. Encrypt via WireGuardManager – identical to import flow:
    //    enables service (was DISABLED), starts it, polls for .conf.dpapi,
    //    stops and disables it again. Manager also removes the plain .conf.
    bool bEncrypted = _EncryptProfileViaMgr(wszConfPath, pwszProfile);
    if (!bEncrypted)
    {
        // Plaintext .conf is still there – clean it up via service pipe
        _SvcDeleteFile(wszConfPath);
        MessageBoxW(_hWnd,
            T(L"Profil wurde geschrieben, konnte aber nicht verschlüsselt werden.\n"
              L"Bitte prüfen Sie den WireGuardManager-Dienst.",
              L"Profile was written but could not be encrypted.\n"
              L"Please check the WireGuardManager service."),
            T(L"Fehler", L"Error"), MB_ICONWARNING | MB_OK | MB_SETFOREGROUND);
        if (bWasConnected) _Connect(_nSelectedProfile);
        return;
    }

    // Manager removes the plain .conf itself; belt-and-suspenders cleanup:
    _SvcDeleteFile(wszConfPath);

    // 4. Reload profile list
    _LoadProfiles();
    _RefreshStatus();
    _UpdateTrayIcon();

    MessageBoxW(_hWnd,
        T(L"Profil erfolgreich gespeichert.", L"Profile saved successfully."),
        T(L"WireGuard Credential Provider", L"WireGuard Credential Provider"),
        MB_ICONINFORMATION | MB_OK | MB_SETFOREGROUND);

    if (bWasConnected)
    {
        int nRecon = MessageBoxW(_hWnd,
            T(L"Soll die VPN-Verbindung jetzt wiederhergestellt werden?",
              L"Do you want to reconnect the VPN now?"),
            T(L"Verbinden?", L"Reconnect?"),
            MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND);
        if (nRecon == IDYES) _Connect(_nSelectedProfile);
    }
}

// ---------------------------------------------------------------------------
// _ExportProfile – decrypt .conf.dpapi and save as plaintext .conf via
//                  a file picker. The user chooses the destination.
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_ExportProfile(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= _nProfiles) return;
    PCWSTR pwszProfile = _rgProfiles[profileIndex];

    // Build source .conf.dpapi path
    WCHAR wszConfigDir[MAX_PATH_WGCP] = {};
    WGGetConfigDir(wszConfigDir, MAX_PATH_WGCP);
    WCHAR wszDpapiPath[MAX_PATH_WGCP] = {};
    StringCchPrintfW(wszDpapiPath, MAX_PATH_WGCP, L"%s%s.conf.dpapi",
                     wszConfigDir, pwszProfile);

    // Decrypt via SYSTEM service pipe (op 3)
    BYTE* pbPlain = nullptr;
    DWORD dwPlainLen = 0;
    if (!_SvcReadDecrypted(wszDpapiPath, &pbPlain, &dwPlainLen))
    {
        MessageBoxW(_hWnd,
            T(L"Entschlüsselung fehlgeschlagen. Bitte prüfen Sie den Hilfsdienst.",
              L"Decryption failed. Please check the helper service."),
            T(L"Fehler", L"Error"), MB_ICONERROR | MB_OK | MB_SETFOREGROUND);
        return;
    }

    // Default filename: <profilename>.conf
    WCHAR wszDest[MAX_PATH_WGCP] = {};
    StringCchPrintfW(wszDest, MAX_PATH_WGCP, L"%s.conf", pwszProfile);

    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner   = _hWnd;
    ofn.lpstrFilter = L"WireGuard Konfiguration (*.conf)\0*.conf\0\0";
    ofn.lpstrFile   = wszDest;
    ofn.nMaxFile    = MAX_PATH_WGCP;
    ofn.lpstrTitle  = T(L"Profil exportieren", L"Export profile");
    ofn.lpstrDefExt = L"conf";
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn))
    {
        // User cancelled
        delete[] pbPlain;
        return;
    }

    // Write plaintext to chosen path
    HANDLE hFile = CreateFileW(wszDest, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD dwErr = GetLastError();
        delete[] pbPlain;
        WCHAR wszErr[128] = {};
        StringCchPrintfW(wszErr, ARRAYSIZE(wszErr),
            T(L"Datei konnte nicht geschrieben werden (Fehler %lu).",
              L"Could not write file (error %lu)."), dwErr);
        MessageBoxW(_hWnd, wszErr, T(L"Fehler", L"Error"),
                    MB_ICONERROR | MB_OK | MB_SETFOREGROUND);
        return;
    }

    DWORD dwWritten = 0;
    WriteFile(hFile, pbPlain, dwPlainLen, &dwWritten, nullptr);
    CloseHandle(hFile);
    delete[] pbPlain;

    if (dwWritten != dwPlainLen)
    {
        MessageBoxW(_hWnd,
            T(L"Export unvollständig – Datei möglicherweise beschädigt.",
              L"Export incomplete – file may be corrupted."),
            T(L"Warnung", L"Warning"), MB_ICONWARNING | MB_OK | MB_SETFOREGROUND);
        return;
    }

    WCHAR wszMsg[MAX_PATH_WGCP + 64] = {};
    StringCchPrintfW(wszMsg, ARRAYSIZE(wszMsg),
        T(L"Profil „%s“ wurde exportiert.",
          L"Profile “%s” was exported."),
        pwszProfile);
    MessageBoxW(_hWnd, wszMsg,
        T(L"Export erfolgreich", L"Export successful"),
        MB_ICONINFORMATION | MB_OK | MB_SETFOREGROUND);
}

void WireGuardTrayApp::_DeleteProfileAt(int profileIndex)
{
    if (profileIndex < 0 || profileIndex >= _nProfiles)
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64,
            L"DeleteProfileAt: index %d out of range (nProfiles=%d)",
            profileIndex, _nProfiles);
        LOG_WARN(e);
        return;
    }

    // Only block deletion if THIS profile is the one currently connected.
    // Other profiles may be deleted freely even while a tunnel is active.
    bool bThisProfileConnected = (profileIndex == _nSelectedProfile) && _bConnected;
    if (bThisProfileConnected)
    {
        LOG_WARN(L"DeleteProfileAt: rejected – this profile is currently connected");
        MessageBoxW(_hWnd,
            T(L"Dieses Profil ist gerade aktiv verbunden.\nBitte trennen Sie zuerst die VPN-Verbindung.",
              L"This profile is currently connected.\nPlease disconnect the VPN first."),
            T(L"Profil l\u00F6schen", L"Delete Profile"),
            MB_ICONWARNING | MB_OK);
        return;
    }

    PCWSTR pwszProfile = _rgProfiles[profileIndex];

    {
        WCHAR dbg[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(dbg, ARRAYSIZE(dbg),
            L"DeleteProfileAt[%d]: '%s' – awaiting user confirmation",
            profileIndex, pwszProfile);
        LOG_DEBUG(dbg);
    }

    // Confirmation dialog
    WCHAR wszMsg[512] = {};
    StringCchPrintfW(wszMsg, ARRAYSIZE(wszMsg),
        T(L"Profil \u201e%s\u201c wirklich l\u00F6schen?\n\nDiese Aktion kann nicht r\u00FCckg\u00E4ngig gemacht werden.",
          L"Delete profile \u201c%s\u201d?\n\nThis action cannot be undone."),
        pwszProfile);

    int iResult = MessageBoxW(_hWnd, wszMsg,
        T(L"Profil l\u00F6schen", L"Delete Profile"),
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

    if (iResult != IDYES)
    {
        LOG_DEBUG(L"DeleteProfileAt: user cancelled");
        return;
    }

    // Build full path of .conf.dpapi
    WCHAR wszConfigDir[MAX_PATH_WGCP] = {};
    WGGetConfigDir(wszConfigDir, MAX_PATH_WGCP);
    WCHAR wszFile[MAX_PATH_WGCP] = {};
    StringCchPrintfW(wszFile, ARRAYSIZE(wszFile),
        L"%s%s.conf.dpapi", wszConfigDir, pwszProfile);

    {
        WCHAR dLog[MAX_PATH_WGCP + 64] = {};
        StringCchPrintfW(dLog, ARRAYSIZE(dLog),
            L"DeleteProfileAt[%d]: deleting '%s'", profileIndex, wszFile);
        LOG_DEBUG(dLog);
    }

    bool bDeleted = false;

    // Try direct delete first
    if (DeleteFileW(wszFile))
    {
        bDeleted = true;
        WCHAR dOk[MAX_PATH_WGCP + 32] = {};
        StringCchPrintfW(dOk, ARRAYSIZE(dOk),
            L"DeleteProfileAt[%d]: '%s' deleted successfully",
            profileIndex, pwszProfile);
        LOG_DEBUG(dOk);
    }
    else
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64,
            L"DeleteProfileAt[%d]: direct delete failed err=%lu – retrying elevated",
            profileIndex, GetLastError());
        LOG_WARN(e);

        // Fallback: elevated delete via cmd.exe with runas verb
        WCHAR wszCmd[MAX_PATH_WGCP + 32] = {};
        StringCchPrintfW(wszCmd, ARRAYSIZE(wszCmd), L"/C del /F /Q \"%s\"", wszFile);
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb       = L"runas";
        sei.lpFile       = L"cmd.exe";
        sei.lpParameters = wszCmd;
        sei.nShow        = SW_HIDE;
        sei.fMask        = SEE_MASK_NOCLOSEPROCESS;

        if (ShellExecuteExW(&sei))
        {
            if (sei.hProcess)
            {
                WaitForSingleObject(sei.hProcess, 5000);
                CloseHandle(sei.hProcess);
            }
            Sleep(300);
            // Verify the file is gone
            if (GetFileAttributesW(wszFile) == INVALID_FILE_ATTRIBUTES)
            {
                bDeleted = true;
                WCHAR dElev[MAX_PATH_WGCP + 48] = {};
                StringCchPrintfW(dElev, ARRAYSIZE(dElev),
                    L"DeleteProfileAt[%d]: '%s' deleted via elevated cmd",
                    profileIndex, pwszProfile);
                LOG_DEBUG(dElev);
            }
            else
            {
                WCHAR e2[MAX_PATH_WGCP + 48] = {};
                StringCchPrintfW(e2, ARRAYSIZE(e2),
                    L"DeleteProfileAt[%d]: elevated delete ran but file still exists: '%s'",
                    profileIndex, wszFile);
                LOG_CRIT(e2);
            }
        }
        else
        {
            WCHAR e2[64] = {};
            StringCchPrintfW(e2, 64,
                L"DeleteProfileAt[%d]: elevated delete ShellExecuteEx failed err=%lu",
                profileIndex, GetLastError());
            LOG_CRIT(e2);
        }
    }

    if (!bDeleted)
    {
        MessageBoxW(_hWnd,
            T(L"Das Profil konnte nicht gel\u00F6scht werden.\nBitte pr\u00FCfen Sie die Berechtigungen.",
              L"The profile could not be deleted.\nPlease check the permissions."),
            T(L"Fehler", L"Error"),
            MB_ICONERROR | MB_OK);
        LOG_CRIT(L"DeleteProfileAt: deletion failed – aborting reload");
        return;
    }

    // If we just deleted the currently active profile, reset selection
    if (profileIndex == _nSelectedProfile)
    {
        LOG_DEBUG(L"DeleteProfileAt: deleted profile was active – resetting selection to 0");
        _nSelectedProfile = 0;
    }
    else if (profileIndex < _nSelectedProfile)
    {
        // Shift selection down to keep the same profile active
        _nSelectedProfile--;
        WCHAR dbg2[64] = {};
        StringCchPrintfW(dbg2, 64,
            L"DeleteProfileAt: shifted _nSelectedProfile to %d", _nSelectedProfile);
        LOG_DEBUG(dbg2);
    }

    Sleep(300);
    _LoadProfiles();
    _RefreshStatus();
    _UpdateTrayIcon();

    {
        WCHAR dbgDone[MAX_PATH_WGCP + 32] = {};
        StringCchPrintfW(dbgDone, ARRAYSIZE(dbgDone),
            L"DeleteProfileAt: reload complete, nProfiles now %d", _nProfiles);
        LOG_DEBUG(dbgDone);
    }

    WCHAR wszDone[256] = {};
    StringCchPrintfW(wszDone, ARRAYSIZE(wszDone),
        T(L"Profil \u201e%s\u201c wurde gel\u00F6scht.",
          L"Profile \u201c%s\u201d has been deleted."),
        pwszProfile);
    _ShowBalloon(
        T(L"Profil gel\u00F6scht", L"Profile deleted"),
        wszDone, NIIF_INFO);
}

// ---------------------------------------------------------------------------
// _DeleteProfile – legacy wrapper, deletes currently selected profile.
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_DeleteProfile()
{
    LOG_DEBUG(L"DeleteProfile: legacy call -> delegating to _DeleteProfileAt(_nSelectedProfile)");
    _DeleteProfileAt(_nSelectedProfile);
}

// ---------------------------------------------------------------------------
// _OpenYubiKeyManager – launch YubiKey Manager GUI as administrator
// ---------------------------------------------------------------------------
void WireGuardTrayApp::_OpenYubiKeyManager()
{
    if (!_wszYkMgrPath[0]) { LOG_WARN(L"OpenYkManager: no path set"); return; }
    LOG_DEBUG(L"OpenYkManager: launching YubiKey Manager as admin");

    // Launch with CreateProcess using the tray's own token.
    // ShellExecute runas is unreliable when UAC is disabled.
    WCHAR wszExeDir[MAX_PATH] = {};
    StringCchCopyW(wszExeDir, MAX_PATH, _wszYkMgrPath);
    WCHAR* pSlash = wcsrchr(wszExeDir, L'\\');
    if (pSlash) *pSlash = L'\0';

    WCHAR wszCmd[MAX_PATH + 4] = {};
    StringCchPrintfW(wszCmd, ARRAYSIZE(wszCmd), L"\"%s\"", _wszYkMgrPath);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;
    PROCESS_INFORMATION pi = {};
    if (CreateProcessW(nullptr, wszCmd, nullptr, nullptr, FALSE,
                       0, nullptr, wszExeDir, &si, &pi))
    {
        LOG_DEBUG(L"OpenYkManager: launched successfully");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else
    {
        // Fallback: ShellExecuteEx with open verb
        LOG_WARN(L"OpenYkManager: CreateProcess failed, trying ShellExecuteEx");
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask   = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_UNICODE;
        sei.lpVerb  = L"open";
        sei.lpFile  = _wszYkMgrPath;
        sei.nShow   = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&sei))
        {
            LOG_WARN(L"OpenYkManager: ShellExecuteEx also failed");
            WCHAR e[256] = {};
            StringCchPrintfW(e, 256,
                T(L"Yubico Authenticator konnte nicht ge\u00F6ffnet werden.\n%s",
                  L"Could not open Yubico Authenticator.\n%s"),
                _wszYkMgrPath);
            MessageBoxW(_hWnd, e,
                T(L"Fehler", L"Error"), MB_OK | MB_ICONERROR);
        }
        else
        {
            LOG_DEBUG(L"OpenYkManager: launched via ShellExecuteEx");
            if (sei.hProcess) CloseHandle(sei.hProcess);
        }
    }
}


// ---------------------------------------------------------------------------
// _GetDisplayVersion – Version aus Windows-Uninstall-Key lesen
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// _GetDisplayVersion
// Priority: 1) Installer registry (DisplayVersion)
//           2) EXE file version resource (FILEVERSION)
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// _GetDisplayVersion
// Priority: 1) Installer registry  2) EXE FILEVERSION resource
// ---------------------------------------------------------------------------
static void _GetDisplayVersion(PWSTR pwszBuf, int cchBuf)
{
    // 1) Main app registry key (written by installer as "Version" = VERSION_DISP)
    //    Prefer this over the Uninstall key: the Uninstall GUID changes between
    //    installs but WGCP_REG_KEY is stable.
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, WGCP_REG_KEY, 0,
                      KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD cbData = (DWORD)(cchBuf * sizeof(WCHAR));
        LONG lRet = RegQueryValueExW(hKey, L"Version", nullptr, nullptr,
                                     (LPBYTE)pwszBuf, &cbData);
        RegCloseKey(hKey);
        if (lRet == ERROR_SUCCESS && pwszBuf[0] != L'\0')
            return;
    }

    // 2) EXE file version resource
    WCHAR wszExe[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, wszExe, MAX_PATH) > 0)
    {
        DWORD dwDummy = 0;
        DWORD dwSize  = GetFileVersionInfoSizeW(wszExe, &dwDummy);
        if (dwSize > 0)
        {
            BYTE* pVer = new(std::nothrow) BYTE[dwSize];
            if (pVer && GetFileVersionInfoW(wszExe, 0, dwSize, pVer))
            {
                VS_FIXEDFILEINFO* pFI = nullptr;
                UINT uLen = 0;
                if (VerQueryValueW(pVer, L"\\",
                                   reinterpret_cast<LPVOID*>(&pFI), &uLen) && pFI)
                {
                    StringCchPrintfW(pwszBuf, cchBuf, L"%u.%u.%u",
                        HIWORD(pFI->dwProductVersionMS),
                        LOWORD(pFI->dwProductVersionMS),
                        HIWORD(pFI->dwProductVersionLS));
                    delete[] pVer;
                    return;
                }
            }
            delete[] pVer;
        }
    }

    StringCchCopyW(pwszBuf, cchBuf, WGCP_VERSION_FALLBACK);
}

// ---------------------------------------------------------------------------
// About-Dialog  (eigenes Fenster, kein .rc-Template)
//
// Layout (420 x 270 px):
//   [0..56]   Dunkelblauer Header-Streifen  (#1A376E)
//   [56..64]  1px dunkle Linie
//   [68]      Icon (32x32) + Version-Label (bold) + Copyright
//   [116]     Beschreibungstext (2 Zeilen)
//   [158]     GitHub-SysLink
//   [214]     Trennlinie
//   [224]     OK-Button (zentriert)
// ---------------------------------------------------------------------------

// Control-IDs (lokale Konstanten)
enum { IDC_ABT_ICON=10, IDC_ABT_VER, IDC_ABT_CPY, IDC_ABT_DESC, IDC_ABT_LINK, IDC_ABT_SEP };

struct AboutDlgData { PCWSTR ver; PCWSTR url; HWND hParent; HINSTANCE hInst; };

// Schriftarten – einmal beim WM_CREATE erstellt, bei WM_DESTROY freigegeben
struct AboutFonts { HFONT hBold; HFONT hNormal; };

static LRESULT CALLBACK _AboutWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static AboutDlgData* s_pd    = nullptr;
    static AboutFonts    s_fonts = {};
    static HBRUSH        s_hbrDialog = nullptr;  // COLOR_BTNFACE brush

    switch (msg)
    {
    // ------------------------------------------------------------------
    case WM_CREATE:
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        s_pd = reinterpret_cast<AboutDlgData*>(cs->lpCreateParams);

        // Fonts
        HDC hdc = GetDC(hWnd);
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(hWnd, hdc);
        s_fonts.hBold   = CreateFontW(-MulDiv(10,dpi,72),0,0,0,FW_BOLD,  FALSE,FALSE,FALSE,
                                       DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
        s_fonts.hNormal = CreateFontW(-MulDiv(10,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                                       DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
        s_hbrDialog = GetSysColorBrush(COLOR_BTNFACE);

        // Layout constants – all y values are client-area offsets
        // Header strip: y=0..56 (painted in WM_PAINT)
        // Body starts at y=66
        const int PAD  = 16;
        const int X0   = PAD;
        const int W_CT = 388;  // content width (420 - 2*PAD)

        // -- Icon (32x32, y=68) --
        HWND hIco = CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD|WS_VISIBLE|SS_ICON|SS_CENTERIMAGE,
            X0, 68, 32, 32, hWnd,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ABT_ICON)),
            s_pd->hInst, nullptr);
        HICON hIcon = static_cast<HICON>(LoadImageW(nullptr, IDI_INFORMATION,
            IMAGE_ICON, 32, 32, LR_SHARED));
        SendMessageW(hIco, STM_SETICON, reinterpret_cast<WPARAM>(hIcon), 0);

        // -- Version (bold, beside icon) --
        WCHAR wszVer[128] = {};
        StringCchPrintfW(wszVer, ARRAYSIZE(wszVer), L"Version %s", s_pd->ver);
        HWND hVer = CreateWindowExW(0, L"STATIC", wszVer,
            WS_CHILD|WS_VISIBLE|SS_LEFT,
            60, 70, 330, 20, hWnd,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ABT_VER)),
            s_pd->hInst, nullptr);
        SendMessageW(hVer, WM_SETFONT, reinterpret_cast<WPARAM>(s_fonts.hBold), TRUE);

        // -- Copyright --
        HWND hCpy = CreateWindowExW(0, L"STATIC", L"\u00A9 2026 Jens Kaesler",
            WS_CHILD|WS_VISIBLE|SS_LEFT,
            60, 92, 330, 18, hWnd,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ABT_CPY)),
            s_pd->hInst, nullptr);
        SendMessageW(hCpy, WM_SETFONT, reinterpret_cast<WPARAM>(s_fonts.hNormal), TRUE);

        // -- Separator line (thin, y=120) --
        CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD|WS_VISIBLE|SS_ETCHEDHORZ,
            X0, 122, W_CT, 1, hWnd,
            reinterpret_cast<HMENU>(6), s_pd->hInst, nullptr);

        // -- Description (y=132) --
        PCWSTR pwszDesc = T(
            L"WireGuard-Anmeldeanbieter f\u00FCr Windows-Dom\u00E4nen\r\n"
            L"mit YubiKey / Smartcard-Unterst\u00FCtzung.",
            L"WireGuard credential provider for Windows domains\r\n"
            L"with YubiKey / smartcard support.");
        HWND hDesc = CreateWindowExW(0, L"STATIC", pwszDesc,
            WS_CHILD|WS_VISIBLE|SS_LEFT,
            X0, 132, W_CT, 36, hWnd,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ABT_DESC)),
            s_pd->hInst, nullptr);
        SendMessageW(hDesc, WM_SETFONT, reinterpret_cast<WPARAM>(s_fonts.hNormal), TRUE);

        // -- GitHub SysLink (y=180) --
        WCHAR wszLink[512] = {};
        StringCchPrintfW(wszLink, ARRAYSIZE(wszLink),
            L"<a href=\"%s\">\U0001F517 github.com/jenskaesler/wireguard_credential_provider</a>",
            s_pd->url);
        HWND hLink = CreateWindowExW(0, WC_LINK, wszLink,
            WS_CHILD|WS_VISIBLE|WS_TABSTOP,
            X0, 180, W_CT, 24, hWnd,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ABT_LINK)),
            s_pd->hInst, nullptr);
        SendMessageW(hLink, WM_SETFONT, reinterpret_cast<WPARAM>(s_fonts.hNormal), TRUE);

        // -- Bottom separator (y=222) --
        CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD|WS_VISIBLE|SS_ETCHEDHORZ,
            0, 222, 420, 2, hWnd,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_ABT_SEP)),
            s_pd->hInst, nullptr);

        // -- OK button (centered, y=232) --
        HWND hOK = CreateWindowExW(0, L"BUTTON",
            T(L"OK", L"OK"),
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
            165, 232, 90, 28, hWnd,
            reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDOK)),
            s_pd->hInst, nullptr);
        SendMessageW(hOK, WM_SETFONT, reinterpret_cast<WPARAM>(s_fonts.hNormal), TRUE);

        return 0;
    }

    // ------------------------------------------------------------------
    case WM_DESTROY:
        if (s_fonts.hBold)   { DeleteObject(s_fonts.hBold);   s_fonts.hBold   = nullptr; }
        if (s_fonts.hNormal) { DeleteObject(s_fonts.hNormal); s_fonts.hNormal = nullptr; }
        return 0;

    // ------------------------------------------------------------------
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Dark blue header strip
        RECT rcHdr = { 0, 0, 420, 56 };
        HBRUSH hbrHdr = CreateSolidBrush(RGB(0x1a, 0x37, 0x6e));
        FillRect(hdc, &rcHdr, hbrHdr);
        DeleteObject(hbrHdr);

        // 1px border line below header
        RECT rcLine = { 0, 56, 420, 57 };
        HBRUSH hbrLine = CreateSolidBrush(RGB(0x0d, 0x1c, 0x40));
        FillRect(hdc, &rcLine, hbrLine);
        DeleteObject(hbrLine);

        // App title in header (Segoe UI Bold 14pt, white)
        HDC hdcTmp = GetDC(hWnd);
        int dpi = GetDeviceCaps(hdcTmp, LOGPIXELSY);
        ReleaseDC(hWnd, hdcTmp);
        HFONT hFTitle = CreateFontW(
            -MulDiv(14, dpi, 72), 0,0,0, FW_BOLD, FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH|FF_DONTCARE, L"Segoe UI");
        HFONT hOld = static_cast<HFONT>(SelectObject(hdc, hFTitle));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0xFF, 0xFF, 0xFF));
        RECT rcTitle = { 16, 0, 404, 56 };
        DrawTextW(hdc, L"WireGuard Credential Provider", -1,
                  &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, hOld);
        DeleteObject(hFTitle);

        EndPaint(hWnd, &ps);
        return 0;
    }

    // ------------------------------------------------------------------
    // Make all child static controls transparent (no colored background box)
    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        SetTextColor(reinterpret_cast<HDC>(wParam), GetSysColor(COLOR_WINDOWTEXT));
        return reinterpret_cast<LRESULT>(s_hbrDialog ? s_hbrDialog
                                                      : GetStockObject(NULL_BRUSH));

    // ------------------------------------------------------------------
    case WM_NOTIFY:
    {
        auto* pNM = reinterpret_cast<NMHDR*>(lParam);
        if (pNM->idFrom == IDC_ABT_LINK && pNM->code == NM_CLICK)
        {
            auto* pLink = reinterpret_cast<NMLINK*>(lParam);
            ShellExecuteW(nullptr, L"open", pLink->item.szUrl,
                          nullptr, nullptr, SW_SHOWNORMAL);
        }
        return 0;
    }

    // ------------------------------------------------------------------
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            if (s_pd) EnableWindow(s_pd->hParent, TRUE);
            DestroyWindow(hWnd);
        }
        return 0;

    case WM_CLOSE:
        if (s_pd) EnableWindow(s_pd->hParent, TRUE);
        DestroyWindow(hWnd);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void WireGuardTrayApp::_ShowAboutDialog()
{
    WCHAR wszVersion[64] = {};
    _GetDisplayVersion(wszVersion, ARRAYSIZE(wszVersion));

    // Register window class (idempotent)
    WNDCLASSEXW wc    = {};
    wc.cbSize         = sizeof(wc);
    wc.lpfnWndProc    = _AboutWndProc;
    wc.hInstance      = _hInst;
    wc.hCursor        = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground  = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName  = L"WGCPAboutDlg";
    RegisterClassExW(&wc);

    // Fixed dialog size (non-resizable)
    const int W = 420, H = 272;
    RECT rcAdj = { 0, 0, W, H };
    AdjustWindowRectEx(&rcAdj, WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    int wW = rcAdj.right  - rcAdj.left;
    int wH = rcAdj.bottom - rcAdj.top;
    int scW = GetSystemMetrics(SM_CXSCREEN);
    int scH = GetSystemMetrics(SM_CYSCREEN);

    AboutDlgData dlgData = { wszVersion, WGCP_GITHUB_URL, _hWnd, _hInst };

    EnableWindow(_hWnd, FALSE);
    HWND hAbout = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"WGCPAboutDlg",
        T(L"WireGuard Credential Provider – Informationen",
          L"WireGuard Credential Provider – About"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        (scW - wW) / 2, (scH - wH) / 2, wW, wH,
        _hWnd, nullptr, _hInst, &dlgData);

    if (!hAbout) { EnableWindow(_hWnd, TRUE); return; }
    ShowWindow(hAbout, SW_SHOW);
    UpdateWindow(hAbout);

    // Modal message loop
    MSG m = {};
    while (IsWindow(hAbout) && GetMessageW(&m, nullptr, 0, 0) > 0)
    {
        if (!IsWindow(hAbout)) break;
        if (IsDialogMessageW(hAbout, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    if (m.message == WM_QUIT)
        PostQuitMessage(static_cast<int>(m.wParam));
}


void WireGuardTrayApp::_StartUpdateCheckThread()
{
    if (_hUpdateThread) return;  // läuft bereits
    _hUpdateStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!_hUpdateStop) return;
    _hUpdateThread = CreateThread(nullptr, 0, _UpdateCheckThread, this, 0, nullptr);
    if (!_hUpdateThread) { CloseHandle(_hUpdateStop); _hUpdateStop = nullptr; }
    else LOG_DEBUG(L"UpdateCheck: thread started");
}

void WireGuardTrayApp::_StopUpdateCheckThread()
{
    if (_hUpdateStop)  SetEvent(_hUpdateStop);
    if (_hUpdateThread)
    {
        WaitForSingleObject(_hUpdateThread, 5000);
        CloseHandle(_hUpdateThread);
        _hUpdateThread = nullptr;
    }
    if (_hUpdateStop)  { CloseHandle(_hUpdateStop); _hUpdateStop = nullptr; }
}

// Hilfsfunktion: JSON-String-Wert extrahieren ("key":"value")
static bool ExtractJsonString(const char* pszJson, const char* pszKey,
                               char* pszOut, int cchOut)
{
    // Suche "key":"
    const char* p = strstr(pszJson, pszKey);
    if (!p) return false;
    p += strlen(pszKey);
    // Überspringe optionale Leerzeichen und ':'
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return false;
    p++;  // öffnendes "
    int i = 0;
    while (*p && *p != '"' && i < cchOut - 1)
        pszOut[i++] = *p++;
    pszOut[i] = '\0';
    return i > 0;
}

// Versionsnummer-Vergleich: "2026.8.5" > "2026.8.4" → true
// Format: YYYY.M.R (keine führenden Nullen, kein 'v'-Präfix)
static bool IsNewerVersion(const char* pszLatest, const char* pszInstalled)
{
    // 'v'-Präfix tolerieren (GitHub tag_name ist oft "v2026.8.5")
    if (pszLatest[0] == 'v') pszLatest++;
    int la=0, lb=0, lc=0;
    int ia=0, ib=0, ic=0;
    if (sscanf_s(pszLatest,   "%d.%d.%d", &la, &lb, &lc) < 2) return false;
    if (sscanf_s(pszInstalled, "%d.%d.%d", &ia, &ib, &ic) < 2) return false;
    if (la != ia) return la > ia;
    if (lb != ib) return lb > ib;
    return lc > ic;
}

DWORD WINAPI WireGuardTrayApp::_UpdateCheckThread(LPVOID lpParam)
{
    WireGuardTrayApp* pApp = static_cast<WireGuardTrayApp*>(lpParam);

    // 10 Minuten warten (in 1-Sekunden-Schritten für schnellen Stop)
    const DWORD WAIT_TOTAL_MS = 10 * 60 * 1000;
    DWORD dwWaited = 0;
    while (dwWaited < WAIT_TOTAL_MS)
    {
        if (WaitForSingleObject(pApp->_hUpdateStop, 1000) != WAIT_TIMEOUT) return 0;
        dwWaited += 1000;
    }

    LOG_DEBUG(L"UpdateCheck: checking GitHub for latest release...");

    // --- Installierte Version lesen ---
    char szInstalled[64] = "(unknown)";
    {
        WCHAR wszVer[64] = {};
        _GetDisplayVersion(wszVer, ARRAYSIZE(wszVer));
        WideCharToMultiByte(CP_ACP, 0, wszVer, -1, szInstalled, ARRAYSIZE(szInstalled), nullptr, nullptr);
    }

    // --- WinHTTP: GET api.github.com ---
    HINTERNET hSession = WinHttpOpen(
        L"WireGuardCP-UpdateCheck/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { LOG_WARN(L"UpdateCheck: WinHttpOpen failed"); return 0; }

    HINTERNET hConnect = WinHttpConnect(hSession,
        L"api.github.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); LOG_WARN(L"UpdateCheck: WinHttpConnect failed"); return 0; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
        L"GET",
        L"/repos/jenskaesler/wireguard_credential_provider/releases/latest",
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        LOG_WARN(L"UpdateCheck: WinHttpOpenRequest failed");
        return 0;
    }

    // User-Agent und Accept-Header setzen
    WinHttpAddRequestHeaders(hRequest,
        L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28",
        (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    BOOL bSent = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!bSent || !WinHttpReceiveResponse(hRequest, nullptr))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        LOG_WARN(L"UpdateCheck: HTTP request failed");
        return 0;
    }

    // HTTP-Status prüfen
    DWORD dwStatus = 0;
    DWORD cbStatus = sizeof(dwStatus);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &dwStatus, &cbStatus, WINHTTP_NO_HEADER_INDEX);
    if (dwStatus != 200)
    {
        WCHAR wszWarn[64];
        StringCchPrintfW(wszWarn, ARRAYSIZE(wszWarn),
            L"UpdateCheck: HTTP status %lu", dwStatus);
        LOG_WARN(wszWarn);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return 0;
    }

    // Response-Body lesen (max. 64 KB)
    char szBody[65536] = {};
    DWORD dwRead = 0, dwOffset = 0;
    while (dwOffset < sizeof(szBody) - 1)
    {
        DWORD dwAvail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwAvail) || dwAvail == 0) break;
        DWORD dwToRead = dwAvail < (DWORD)(sizeof(szBody) - 1 - dwOffset) ? dwAvail : (DWORD)(sizeof(szBody) - 1 - dwOffset);
        if (!WinHttpReadData(hRequest, szBody + dwOffset, dwToRead, &dwRead) || dwRead == 0) break;
        dwOffset += dwRead;
    }
    szBody[dwOffset] = '\0';

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // --- tag_name extrahieren ---
    char szTagName[64] = {};
    if (!ExtractJsonString(szBody, "\"tag_name\"", szTagName, ARRAYSIZE(szTagName)))
    {
        LOG_WARN(L"UpdateCheck: could not parse tag_name from response");
        return 0;
    }

    WCHAR wszDbg[128];
    StringCchPrintfW(wszDbg, ARRAYSIZE(wszDbg),
        L"UpdateCheck: latest=%hs  installed=%hs", szTagName, szInstalled);
    LOG_DEBUG(wszDbg);

    // --- Versionsvergleich ---
    if (!IsNewerVersion(szTagName, szInstalled))
    {
        LOG_DEBUG(L"UpdateCheck: already up to date");
        return 0;
    }

    // --- Release-URL bauen: "v"-Präfix tolerieren ---
    const char* pszTag = (szTagName[0] == 'v') ? szTagName + 1 : szTagName;
    char szUrl[256] = {};
    sprintf_s(szUrl, ARRAYSIZE(szUrl),
        "https://github.com/jenskaesler/wireguard_credential_provider/releases/tag/%s",
        (szTagName[0] == 'v') ? szTagName : szTagName);
    MultiByteToWideChar(CP_ACP, 0, szUrl, -1,
        pApp->_wszUpdateUrl, ARRAYSIZE(pApp->_wszUpdateUrl));

    // --- Balloon-Tip anzeigen ---
    WCHAR wszTitle[128], wszMsg[256];
    const char* pszDisplay = (szTagName[0] == 'v') ? szTagName + 1 : szTagName;
    StringCchPrintfW(wszTitle, ARRAYSIZE(wszTitle),
        T(L"\U0001F504 Update verf\u00FCgbar: %hs",
          L"\U0001F504 Update available: %hs"),
        pszDisplay);
    StringCchPrintfW(wszMsg, ARRAYSIZE(wszMsg),
        T(L"Version %hs ist verf\u00FCgbar. Hier klicken zum Download.",
          L"Version %hs is available. Click here to download."),
        pszDisplay);

    pApp->_bUpdateBalloonActive = true;
    pApp->_ShowBalloon(wszTitle, wszMsg, NIIF_INFO, 15000);

    LOG_DEBUG(L"UpdateCheck: balloon shown");
    return 0;
}