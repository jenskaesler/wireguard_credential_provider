#include "WireGuardCredential.h"
#include "WireGuardProvider.h"
#include "FieldDescriptors.h"
#include <new>

// ---------------------------------------------------------------------------
// Felddefinitionen - KEINE 4-Byte Emoji, nur BMP Unicode (max \uFFFF)
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
WireGuardCredential::WireGuardCredential()
    : _cRef(1), _cpus(CPUS_INVALID), _pCredProvCredentialEvents(nullptr)
    , _nProfiles(0), _dwSelectedProfile(0), _bConnected(false)
    , _bSelected(false), _hTimerThread(nullptr), _bStopTimer(false)
    , _pProvider(nullptr), _hShutdownThread(nullptr), _hShutdownWnd(nullptr)
{
    ZeroMemory(_rgCredProvFieldDescriptors, sizeof(_rgCredProvFieldDescriptors));
    ZeroMemory(_rgFieldStatePairs,          sizeof(_rgFieldStatePairs));
    ZeroMemory(_rgProfiles,                 sizeof(_rgProfiles));
    ZeroMemory(_wszExePath,                 sizeof(_wszExePath));
    ZeroMemory(_wszWgExePath,               sizeof(_wszWgExePath));
    ZeroMemory(_wszIconConn,                sizeof(_wszIconConn));
    ZeroMemory(_wszIconDisconn,             sizeof(_wszIconDisconn));
    ZeroMemory(_wszStatus,                  sizeof(_wszStatus));
    ZeroMemory(_wszTraffic,                 sizeof(_wszTraffic));
}

WireGuardCredential::~WireGuardCredential()
{
    // Timer-Thread stoppen
    _bStopTimer = true;
    if (_hTimerThread)
    {
        WaitForSingleObject(_hTimerThread, 3000);
        CloseHandle(_hTimerThread);
        _hTimerThread = nullptr;
    }
    // Shutdown-Listener-Fenster schliessen
    if (_hShutdownWnd)
    {
        PostMessageW(_hShutdownWnd, WM_CLOSE, 0, 0);
        _hShutdownWnd = nullptr;
    }
    if (_hShutdownThread)
    {
        WaitForSingleObject(_hShutdownThread, 3000);
        CloseHandle(_hShutdownThread);
        _hShutdownThread = nullptr;
    }
    for (int i = 0; i < FI_NUM_FIELDS; i++)
        CoTaskMemFree(_rgCredProvFieldDescriptors[i].pszLabel);
}

// ---------------------------------------------------------------------------
HRESULT WireGuardCredential::Initialize(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
    const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* rgcpfd,
    const FIELD_STATE_PAIR* rgfsp)
{
    LOG_DEBUG(L"=== WireGuardCredential::Initialize v2 ===");
    _cpus = cpus;

    HRESULT hr = S_OK;
    for (DWORD i = 0; i < FI_NUM_FIELDS && SUCCEEDED(hr); i++)
    {
        _rgFieldStatePairs[i]           = rgfsp[i];
        _rgCredProvFieldDescriptors[i]  = rgcpfd[i];
        _rgCredProvFieldDescriptors[i].pszLabel = nullptr;
        if (rgcpfd[i].pszLabel)
            hr = WGCPStrDup(rgcpfd[i].pszLabel, &_rgCredProvFieldDescriptors[i].pszLabel);
    }

    if (SUCCEEDED(hr))
    {
        _LoadConfig();
        _LoadProfiles();

        // Log-Rotation: alte Logs beim Start bereinigen
        {
            WCHAR wszLogPath[MAX_PATH_WGCP] = {};
            DWORD dwRetention = WGCP_DEFAULT_LOGRETENTION;
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, WGCP_REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
            {
                dwRetention = ReadRegDword(hKey, WGCP_REG_LOGRETENTION, WGCP_DEFAULT_LOGRETENTION);
                RegCloseKey(hKey);
            }
            WGCPResolvLogPath(wszLogPath, MAX_PATH_WGCP);
            WGCPRotateLogs(wszLogPath, dwRetention);
        }

        _RefreshStatus();

        // Shutdown-Listener starten
        _hShutdownThread = CreateThread(nullptr, 0, _ShutdownThreadProc, this, 0, nullptr);
        if (_hShutdownThread)
            LOG_DEBUG(L"Shutdown-Listener gestartet");
        else
            LOG_WARN(L"Shutdown-Listener konnte nicht gestartet werden");
    }
    else
    {
        WCHAR e[64]={};
        StringCchPrintfW(e, 64, L"Initialize fehlgeschlagen: 0x%08X", hr);
        LOG_CRIT(e);
    }
    return hr;
}

// ---------------------------------------------------------------------------
void WireGuardCredential::_LoadConfig()
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, WGCP_REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        ReadRegString(hKey, WGCP_REG_EXEPATH,     _wszExePath,     MAX_PATH_WGCP, WGCP_DEFAULT_EXEPATH);
        ReadRegString(hKey, WGCP_REG_WGEXEPATH,   _wszWgExePath,   MAX_PATH_WGCP, WGCP_DEFAULT_WGEXEPATH);
        ReadRegString(hKey, WGCP_REG_ICONCONN,    _wszIconConn,    MAX_PATH_WGCP, WGCP_DEFAULT_ICONCONN);
        ReadRegString(hKey, WGCP_REG_ICONDISCONN, _wszIconDisconn, MAX_PATH_WGCP, WGCP_DEFAULT_ICONDISCONN);
        RegCloseKey(hKey);
        LOG_DEBUG(L"Registry-Konfig geladen");
    }
    else
    {
        LOG_WARN(L"Registry-Key fehlt, verwende Defaults");
        StringCchCopyW(_wszExePath,     MAX_PATH_WGCP, WGCP_DEFAULT_EXEPATH);
        StringCchCopyW(_wszWgExePath,   MAX_PATH_WGCP, WGCP_DEFAULT_WGEXEPATH);
        StringCchCopyW(_wszIconConn,    MAX_PATH_WGCP, WGCP_DEFAULT_ICONCONN);
        StringCchCopyW(_wszIconDisconn, MAX_PATH_WGCP, WGCP_DEFAULT_ICONDISCONN);
    }

    WCHAR d[MAX_PATH_WGCP+32]={};
    StringCchPrintfW(d, ARRAYSIZE(d), L"ExePath=%s", _wszExePath);
    LOG_DEBUG(d);
    StringCchPrintfW(d, ARRAYSIZE(d), L"WgExePath=%s", _wszWgExePath);
    LOG_DEBUG(d);
    StringCchPrintfW(d, ARRAYSIZE(d), L"IconConn=%s", _wszIconConn);
    LOG_DEBUG(d);
    StringCchPrintfW(d, ARRAYSIZE(d), L"IconDisconn=%s", _wszIconDisconn);
    LOG_DEBUG(d);
}

// ---------------------------------------------------------------------------
void WireGuardCredential::_LoadProfiles()
{
    _nProfiles = WGEnumProfiles(_rgProfiles, MAX_PROFILES);
    _dwSelectedProfile = 0;

    WCHAR d[128]={};
    StringCchPrintfW(d, 128, L"%d Profile im Verzeichnis gefunden", _nProfiles);
    LOG_DEBUG(d);

    for (int i = 0; i < _nProfiles; i++)
    {
        WCHAR d2[MAX_PATH_WGCP+16]={};
        StringCchPrintfW(d2, ARRAYSIZE(d2), L"  Profil[%d]: %s", i, _rgProfiles[i]);
        LOG_DEBUG(d2);
    }

    if (_nProfiles == 0) return;

    // Computername als Standardprofil
    WCHAR wszComp[MAX_PATH_WGCP]={};
    DWORD dwSize = MAX_PATH_WGCP;
    GetComputerNameW(wszComp, &dwSize);

    WCHAR dc[MAX_PATH_WGCP+32]={};
    StringCchPrintfW(dc, ARRAYSIZE(dc), L"Computername: %s", wszComp);
    LOG_DEBUG(dc);

    for (int i = 0; i < _nProfiles; i++)
    {
        if (_wcsicmp(_rgProfiles[i], wszComp) == 0)
        {
            _dwSelectedProfile = static_cast<DWORD>(i);
            WCHAR d2[MAX_PATH_WGCP+32]={};
            StringCchPrintfW(d2, ARRAYSIZE(d2), L"Standardprofil: %s (Index %d)", wszComp, i);
            LOG_DEBUG(d2);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
void WireGuardCredential::_RefreshStatus()
{
    if (_nProfiles == 0 || _dwSelectedProfile >= static_cast<DWORD>(_nProfiles))
    {
        _bConnected = false;
        StringCchCopyW(_wszStatus,  MAX_LABEL_WGCP, L"\u25CB Kein Profil");
        StringCchCopyW(_wszTraffic, MAX_LABEL_WGCP, L"");
        LOG_DEBUG(L"Kein Profil verfuegbar");
        return;
    }

    PCWSTR pwszProfile = _rgProfiles[_dwSelectedProfile];
    _bConnected = WGIsTunnelConnected(pwszProfile);

    if (_bConnected)
    {
        WCHAR wszTimer[MAX_LABEL_WGCP]={};
        WGGetConnectedSince(pwszProfile, wszTimer, MAX_LABEL_WGCP);
        StringCchCopyW(_wszStatus, MAX_LABEL_WGCP,
                       wszTimer[0] ? wszTimer : L"\u25CF Verbunden");

        WGGetTrafficStats(_wszWgExePath, pwszProfile, _wszTraffic, MAX_LABEL_WGCP);
    }
    else
    {
        StringCchCopyW(_wszStatus,  MAX_LABEL_WGCP, L"\u25CB Getrennt");
        StringCchCopyW(_wszTraffic, MAX_LABEL_WGCP, L"");
    }

    WCHAR d[MAX_PATH_WGCP+128]={};
    StringCchPrintfW(d, ARRAYSIZE(d), L"Status '%s': %s | Traffic: '%s'",
                     pwszProfile, _wszStatus, _wszTraffic);
    LOG_DEBUG(d);
}

// ---------------------------------------------------------------------------
void WireGuardCredential::_UpdateFields()
{
    if (!_pCredProvCredentialEvents) return;

    // Status-Text
    _pCredProvCredentialEvents->SetFieldString(this, FI_STATUS, _wszStatus);

    // Traffic-Zeile
    if (_bConnected && _wszTraffic[0])
    {
        _pCredProvCredentialEvents->SetFieldState(
            this, FI_TRAFFIC, CPFS_DISPLAY_IN_SELECTED_TILE);
        _pCredProvCredentialEvents->SetFieldString(this, FI_TRAFFIC, _wszTraffic);
    }
    else
    {
        _pCredProvCredentialEvents->SetFieldState(this, FI_TRAFFIC, CPFS_HIDDEN);
    }

    // Button-Text
    PCWSTR pwszBtn = (_nProfiles == 0) ? L"Kein Profil"
                   : (_bConnected      ? L"\u23CF  Trennen"
                                       : L"\u25B6  Verbinden");
    _pCredProvCredentialEvents->SetFieldString(this, FI_BUTTON, pwszBtn);
    _pCredProvCredentialEvents->SetFieldInteractiveState(
        this, FI_BUTTON,
        (_nProfiles > 0) ? CPFIS_INTERACTIVE : CPFIS_NONE);
}

// ---------------------------------------------------------------------------
// _LoadBitmap
// Laedt das Bitmap zuerst aus der eingebetteten DLL-Ressource.
// Fallback: Dateipfad aus Registry (fuer eigene Icons).
// ---------------------------------------------------------------------------

// Statische Hilfsfunktion als Anker fuer GetModuleHandleExW
static void _WGCPAnchor() {}

HRESULT WireGuardCredential::_LoadBitmap(PCWSTR pwszPath, HBITMAP* phbmp)
{
    *phbmp = nullptr;

    // Ressource-ID je nach Status
    UINT uResId = _bConnected ? IDB_WIREGUARD_CONNECTED : IDB_WIREGUARD_DISCONNECTED;

    // DLL-Instanz ueber statische Ankerfunktion ermitteln
    HMODULE hMod = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(_WGCPAnchor),
        &hMod);

    if (hMod)
    {
        HBITMAP hBmp = static_cast<HBITMAP>(
            LoadImageW(hMod, MAKEINTRESOURCEW(uResId),
                       IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
        if (hBmp)
        {
            *phbmp = hBmp;
            LOG_DEBUG(_bConnected ? L"Icon aus Ressource: connected" : L"Icon aus Ressource: disconnected");
            return S_OK;
        }
        WCHAR e[64]={};
        StringCchPrintfW(e, 64, L"Ressource %u nicht gefunden, Fehler=%lu", uResId, GetLastError());
        LOG_WARN(e);
    }

    // Fallback: Datei aus Registry-Pfad
    if (pwszPath && pwszPath[0])
    {
        HBITMAP hBmp = static_cast<HBITMAP>(
            LoadImageW(nullptr, pwszPath, IMAGE_BITMAP, 0, 0,
                       LR_LOADFROMFILE | LR_CREATEDIBSECTION));
        if (hBmp)
        {
            *phbmp = hBmp;
            LOG_DEBUG(L"Icon aus Datei geladen");
            return S_OK;
        }
        WCHAR e[MAX_PATH_WGCP+64]={};
        StringCchPrintfW(e, ARRAYSIZE(e), L"LoadImage Datei err=%lu: %s", GetLastError(), pwszPath);
        LOG_WARN(e);
    }

    return S_OK;
}

// ---------------------------------------------------------------------------
// IUnknown
// ---------------------------------------------------------------------------
STDMETHODIMP WireGuardCredential::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(WireGuardCredential, ICredentialProviderCredential),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

STDMETHODIMP_(ULONG) WireGuardCredential::Release()
{
    LONG c = InterlockedDecrement(&_cRef);
    if (!c) delete this;
    return c;
}

// ---------------------------------------------------------------------------
// Advise / UnAdvise
// ---------------------------------------------------------------------------
STDMETHODIMP WireGuardCredential::Advise(ICredentialProviderCredentialEvents* pcpce)
{
    if (_pCredProvCredentialEvents) _pCredProvCredentialEvents->Release();
    _pCredProvCredentialEvents = pcpce;
    if (_pCredProvCredentialEvents) _pCredProvCredentialEvents->AddRef();
    return S_OK;
}

STDMETHODIMP WireGuardCredential::UnAdvise()
{
    if (_pCredProvCredentialEvents)
    {
        _pCredProvCredentialEvents->Release();
        _pCredProvCredentialEvents = nullptr;
    }
    return S_OK;
}

// ---------------------------------------------------------------------------
STDMETHODIMP WireGuardCredential::SetSelected(BOOL* pbAutoLogon)
{
    LOG_DEBUG(L"Kachel ausgewaehlt");
    *pbAutoLogon = FALSE;
    _bSelected = true;
    _RefreshStatus();
    _UpdateFields();

    // Auto-Refresh Timer starten
    _bStopTimer = false;
    if (!_hTimerThread)
    {
        _hTimerThread = CreateThread(nullptr, 0, _TimerThreadProc, this, 0, nullptr);
        LOG_DEBUG(L"Auto-Refresh Timer gestartet");
    }
    return S_OK;
}

STDMETHODIMP WireGuardCredential::SetDeselected()
{
    _bSelected = false;
    _bStopTimer = true;
    if (_hTimerThread)
    {
        WaitForSingleObject(_hTimerThread, 2000);
        CloseHandle(_hTimerThread);
        _hTimerThread = nullptr;
        LOG_DEBUG(L"Auto-Refresh Timer gestoppt");
    }
    return S_OK;
}

// ---------------------------------------------------------------------------
// _DisconnectAllOnBoot
// Wird bei jedem Initialize aufgerufen. Trennt alle Tunnel die vom letzten
// Shutdown uebrig geblieben sind. HWND_MESSAGE empfaengt WM_ENDSESSION
// nicht zuverlaessig, daher sauberer Ansatz: beim naechsten Boot trennen.
// ---------------------------------------------------------------------------
void WireGuardCredential::_DisconnectAllOnBoot()
{
    bool bFoundActive = false;
    for (int i = 0; i < _nProfiles; i++)
    {
        if (WGIsTunnelConnected(_rgProfiles[i]))
        {
            if (!bFoundActive)
            {
                LOG_DEBUG(L"Boot-Cleanup: Aktive Tunnel vom letzten Shutdown gefunden");
                bFoundActive = true;
            }
            WCHAR d[MAX_PATH_WGCP + 32] = {};
            StringCchPrintfW(d, ARRAYSIZE(d),
                             L"Boot-Cleanup: Trenne '%s'", _rgProfiles[i]);
            LOG_DEBUG(d);
            WGDisconnect(_wszExePath, _rgProfiles[i]);
            // Kurz warten bis Service gestoppt
            for (int j = 0; j < 10; j++)
            {
                Sleep(300);
                if (!WGIsTunnelConnected(_rgProfiles[i])) break;
            }
        }
    }
    if (bFoundActive)
        LOG_DEBUG(L"Boot-Cleanup: Abgeschlossen");
    else
        LOG_DEBUG(L"Boot-Cleanup: Keine aktiven Tunnel gefunden");
}

// ---------------------------------------------------------------------------
// Shutdown-Listener
// ---------------------------------------------------------------------------

// Fensterprozedur - laeuft im Shutdown-Thread
LRESULT CALLBACK WireGuardCredential::_ShutdownWndProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_QUERYENDSESSION)
    {
        // Shutdown ankuendigen - wir erlauben ihn immer
        LOG_DEBUG(L"WM_QUERYENDSESSION empfangen");
        return TRUE;
    }

    if (uMsg == WM_ENDSESSION && wParam)
    {
        LOG_DEBUG(L"WM_ENDSESSION: trenne aktive Tunnel");

        // Credential-Objekt aus GWLP_USERDATA holen
        WireGuardCredential* pThis = reinterpret_cast<WireGuardCredential*>(
            GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        if (pThis && pThis->_nProfiles > 0)
        {
            // Alle verbundenen Tunnel trennen
            for (int i = 0; i < pThis->_nProfiles; i++)
            {
                if (WGIsTunnelConnected(pThis->_rgProfiles[i]))
                {
                    WCHAR d[MAX_PATH_WGCP + 32] = {};
                    StringCchPrintfW(d, ARRAYSIZE(d),
                                     L"Shutdown: Trenne Tunnel '%s'",
                                     pThis->_rgProfiles[i]);
                    LOG_DEBUG(d);
                    WGDisconnect(pThis->_wszExePath, pThis->_rgProfiles[i]);
                }
            }
            LOG_DEBUG(L"Shutdown: Alle Tunnel getrennt");
        }
        return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// Thread der das Fenster erstellt und die Message-Loop betreibt
DWORD WINAPI WireGuardCredential::_ShutdownThreadProc(LPVOID lpParam)
{
    WireGuardCredential* pThis = static_cast<WireGuardCredential*>(lpParam);

    // Fensterklasse registrieren
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = _ShutdownWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"WGCPShutdownListener";
    RegisterClassExW(&wc);

    // Top-Level Fenster (nicht HWND_MESSAGE!) - nur so kommt WM_ENDSESSION an
    // Fenster ist 0x0 Pixel gross und nicht sichtbar
    HWND hWnd = CreateWindowExW(
        0, L"WGCPShutdownListener", L"",
        WS_POPUP,           // Kein Rahmen, kein Titel
        0, 0, 0, 0,         // Position und Groesse: 0
        nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);

    if (!hWnd)
    {
        WCHAR e[64] = {};
        StringCchPrintfW(e, 64, L"Shutdown-Fenster err=%lu", GetLastError());
        LOG_WARN(e);
        return 1;
    }

    // Credential-Zeiger im Fenster speichern
    SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    pThis->_hShutdownWnd = hWnd;

    // Fenster explizit versteckt lassen
    ShowWindow(hWnd, SW_HIDE);

    LOG_DEBUG(L"Shutdown-Listener Fenster erstellt (Top-Level, versteckt)");

    // Message-Loop
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DestroyWindow(hWnd);
    UnregisterClassW(L"WGCPShutdownListener", GetModuleHandleW(nullptr));
    LOG_DEBUG(L"Shutdown-Listener beendet");
    return 0;
}

// ---------------------------------------------------------------------------
// Timer-Thread: alle 5 Sekunden Status + Traffic aktualisieren
// ---------------------------------------------------------------------------
DWORD WINAPI WireGuardCredential::_TimerThreadProc(LPVOID lpParam)
{
    WireGuardCredential* pThis = static_cast<WireGuardCredential*>(lpParam);

    while (!pThis->_bStopTimer)
    {
        // 5 Sekunden warten, aber in 500ms Schritten damit Stop schnell reagiert
        for (int i = 0; i < 10 && !pThis->_bStopTimer; i++)
            Sleep(500);

        if (pThis->_bStopTimer) break;
        if (!pThis->_bSelected) break;

        pThis->_RefreshStatus();
        pThis->_UpdateFields();
        LOG_DEBUG(L"Auto-Refresh: Status aktualisiert");
    }
    return 0;
}

// ---------------------------------------------------------------------------
STDMETHODIMP WireGuardCredential::GetFieldState(
    DWORD dwFieldID,
    CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
    CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis)
{
    if (dwFieldID >= FI_NUM_FIELDS) return E_INVALIDARG;

    if (dwFieldID == FI_TRAFFIC)
    {
        *pcpfs  = (_bConnected && _wszTraffic[0])
                  ? CPFS_DISPLAY_IN_SELECTED_TILE : CPFS_HIDDEN;
        *pcpfis = CPFIS_NONE;
        return S_OK;
    }
    if (dwFieldID == FI_BUTTON && _nProfiles == 0)
    {
        *pcpfs  = CPFS_DISPLAY_IN_SELECTED_TILE;
        *pcpfis = CPFIS_NONE;
        return S_OK;
    }
    *pcpfs  = _rgFieldStatePairs[dwFieldID].cpfs;
    *pcpfis = _rgFieldStatePairs[dwFieldID].cpfis;
    return S_OK;
}

// ---------------------------------------------------------------------------
STDMETHODIMP WireGuardCredential::GetStringValue(DWORD dwFieldID, WCHAR** ppwsz)
{
    *ppwsz = nullptr;
    if (dwFieldID >= FI_NUM_FIELDS) return E_INVALIDARG;

    switch (dwFieldID)
    {
    case FI_LABEL:
    {
        WCHAR wszLabel[MAX_LABEL_WGCP]={};
        HKEY hKey=nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,WGCP_REG_KEY,0,KEY_READ,&hKey)==ERROR_SUCCESS)
        { ReadRegString(hKey,WGCP_REG_LABEL,wszLabel,MAX_LABEL_WGCP,WGCP_DEFAULT_LABEL); RegCloseKey(hKey); }
        else StringCchCopyW(wszLabel,MAX_LABEL_WGCP,WGCP_DEFAULT_LABEL);
        return WGCPStrDup(wszLabel, ppwsz);
    }
    case FI_STATUS:  return WGCPStrDup(_wszStatus,  ppwsz);
    case FI_TRAFFIC: return WGCPStrDup(_wszTraffic, ppwsz);
    case FI_BUTTON:
    {
        PCWSTR p = (_nProfiles == 0) ? L"Kein Profil"
                 : (_bConnected      ? L"\u23CF  Trennen"
                                     : L"\u25B6  Verbinden");
        return WGCPStrDup(p, ppwsz);
    }
    default: return WGCPStrDup(L"", ppwsz);
    }
}

// ---------------------------------------------------------------------------
STDMETHODIMP WireGuardCredential::GetBitmapValue(DWORD dwFieldID, HBITMAP* phbmp)
{
    *phbmp = nullptr;
    if (dwFieldID != FI_TILEIMAGE) return E_INVALIDARG;
    LOG_DEBUG(_bConnected ? L"GetBitmapValue: connected icon" : L"GetBitmapValue: disconnected icon");
    return _LoadBitmap(_bConnected ? _wszIconConn : _wszIconDisconn, phbmp);
}

// ---------------------------------------------------------------------------
// ComboBox
// ---------------------------------------------------------------------------
STDMETHODIMP WireGuardCredential::GetComboBoxValueCount(
    DWORD dwFieldID, DWORD* pcItems, DWORD* pdwSelectedItem)
{
    if (dwFieldID != FI_PROFILE) return E_INVALIDARG;
    *pcItems         = static_cast<DWORD>(_nProfiles);
    *pdwSelectedItem = _dwSelectedProfile;
    WCHAR d[64]={}; StringCchPrintfW(d,64,L"ComboBox: %d Items",_nProfiles); LOG_DEBUG(d);
    return S_OK;
}

STDMETHODIMP WireGuardCredential::GetComboBoxValueAt(
    DWORD dwFieldID, DWORD dwItem, WCHAR** ppwszItem)
{
    *ppwszItem = nullptr;
    if (dwFieldID != FI_PROFILE || dwItem >= static_cast<DWORD>(_nProfiles))
        return E_INVALIDARG;
    return WGCPStrDup(_rgProfiles[dwItem], ppwszItem);
}

STDMETHODIMP WireGuardCredential::SetComboBoxSelectedValue(
    DWORD dwFieldID, DWORD dwSelectedItem)
{
    if (dwFieldID != FI_PROFILE || dwSelectedItem >= static_cast<DWORD>(_nProfiles))
        return E_INVALIDARG;
    _dwSelectedProfile = dwSelectedItem;
    WCHAR d[MAX_PATH_WGCP+32]={};
    StringCchPrintfW(d,ARRAYSIZE(d),L"Profil gewaehlt: %s",_rgProfiles[_dwSelectedProfile]);
    LOG_DEBUG(d);
    _RefreshStatus();
    _UpdateFields();
    return S_OK;
}

// ---------------------------------------------------------------------------
// CommandLinkClicked - Verbinden / Trennen
// ---------------------------------------------------------------------------
STDMETHODIMP WireGuardCredential::CommandLinkClicked(DWORD dwFieldID)
{
    if (dwFieldID != FI_BUTTON || _nProfiles == 0) return S_OK;

    PCWSTR pwszProfile = _rgProfiles[_dwSelectedProfile];

    if (_bConnected)
    {
        WCHAR d[MAX_PATH_WGCP+32]={};
        StringCchPrintfW(d,ARRAYSIZE(d),L"Trenne Tunnel: %s",pwszProfile); LOG_DEBUG(d);
        WGDisconnect(_wszExePath, pwszProfile);

        // Warten bis Service wirklich gestoppt (max 6 Sekunden)
        for (int i = 0; i < 12; i++)
        {
            Sleep(500);
            if (!WGIsTunnelConnected(pwszProfile))
            {
                LOG_DEBUG(L"Tunnel-Service gestoppt");
                break;
            }
        }
    }
    else
    {
        WCHAR d[MAX_PATH_WGCP+32]={};
        StringCchPrintfW(d,ARRAYSIZE(d),L"Verbinde Tunnel: %s",pwszProfile); LOG_DEBUG(d);
        WGConnect(_wszExePath, pwszProfile);

        // Warten bis Service laeuft (max 6 Sekunden)
        for (int i = 0; i < 12; i++)
        {
            Sleep(500);
            if (WGIsTunnelConnected(pwszProfile))
            {
                LOG_DEBUG(L"Tunnel-Service gestartet");
                break;
            }
        }
    }
    _RefreshStatus();
    _UpdateFields();
    // Icon-Reload: CredentialsChanged NUR nach echter Verbinden/Trennen-Aktion
    if (_pProvider) _pProvider->NotifyStatusChanged();
    return S_OK;
}

// ---------------------------------------------------------------------------
STDMETHODIMP WireGuardCredential::GetCheckboxValue(DWORD, BOOL*, WCHAR**) { return E_NOTIMPL; }
STDMETHODIMP WireGuardCredential::GetSubmitButtonValue(DWORD, DWORD*)     { return E_NOTIMPL; }
STDMETHODIMP WireGuardCredential::SetStringValue(DWORD, PCWSTR)           { return E_NOTIMPL; }
STDMETHODIMP WireGuardCredential::SetCheckboxValue(DWORD, BOOL)           { return E_NOTIMPL; }

STDMETHODIMP WireGuardCredential::GetSerialization(
    CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION*,
    WCHAR** ppwszOptionalStatusText,
    CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon)
{
    *pcpgsr                  = CPGSR_NO_CREDENTIAL_NOT_FINISHED;
    *ppwszOptionalStatusText = nullptr;
    *pcpsiOptionalStatusIcon = CPSI_NONE;
    return S_OK;
}

STDMETHODIMP WireGuardCredential::ReportResult(
    NTSTATUS, NTSTATUS,
    WCHAR** ppwszOptionalStatusText,
    CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon)
{
    *ppwszOptionalStatusText = nullptr;
    *pcpsiOptionalStatusIcon = CPSI_NONE;
    return S_OK;
}
