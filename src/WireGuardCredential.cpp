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
    , _pProvider(nullptr)
    , _dwPinAttempts(0), _hScWatchThread(nullptr), _bStopScWatch(false)
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
    ZeroMemory(_wszPin,                     sizeof(_wszPin));
    ZeroMemory(_wszScStatus,                sizeof(_wszScStatus));
    ZeroMemory(_wszCurrentReader,           sizeof(_wszCurrentReader));
    ZeroMemory(&_scConfig,                  sizeof(_scConfig));
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
    // Smartcard-Watch-Thread stoppen
    _bStopScWatch = true;
    if (_hScWatchThread)
    {
        WaitForSingleObject(_hScWatchThread, 3000);
        CloseHandle(_hScWatchThread);
        _hScWatchThread = nullptr;
    }
    // PIN sicher löschen
    SecureZeroMemory(_wszPin, sizeof(_wszPin));

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

        // Smartcard-Konfiguration laden
        WGCPLoadSmartcardConfig(_scConfig);
        if (_scConfig.bEnabled)
        {
            LOG_DEBUG(L"Smartcard-Authentifizierung aktiviert");
            // SC-Status-Feld sichtbar machen
            _rgFieldStatePairs[FI_SC_STATUS].cpfs = CPFS_DISPLAY_IN_SELECTED_TILE;
            if (_scConfig.bPinRequired)
                _rgFieldStatePairs[FI_PIN].cpfs = CPFS_DISPLAY_IN_SELECTED_TILE;
            StringCchCopyW(_wszScStatus, MAX_LABEL_WGCP,
                           L"\U0001F511 Bitte YubiKey / Smartcard einstecken...");
        }

        // Log-Rotation
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

        // Smartcard-Watch-Thread starten (Connect/Disconnect on Insert/Remove)
        if (_scConfig.bEnabled &&
            (_scConfig.bConnectOnInsert || _scConfig.bDisconnectOnRemove))
        {
            _hScWatchThread = CreateThread(nullptr, 0, _ScWatchThreadProc, this, 0, nullptr);
            if (_hScWatchThread)
                LOG_DEBUG(L"Smartcard-Watch-Thread gestartet");
        }
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
    case FI_STATUS:    return WGCPStrDup(_wszStatus,   ppwsz);
    case FI_TRAFFIC:   return WGCPStrDup(_wszTraffic,  ppwsz);
    case FI_SC_STATUS: return WGCPStrDup(_wszScStatus, ppwsz);
    case FI_PIN:       return WGCPStrDup(L"",          ppwsz);  // Passwortfeld: leer zurückgeben
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
// SetStringValue - PIN-Eingabe speichern
// ---------------------------------------------------------------------------
STDMETHODIMP WireGuardCredential::SetStringValue(DWORD dwFieldID, PCWSTR pwz)
{
    if (dwFieldID == FI_PIN && pwz)
    {
        StringCchCopyW(_wszPin, ARRAYSIZE(_wszPin), pwz);
        LOG_DEBUG(L"SC: PIN-Eingabe empfangen");
    }
    return S_OK;
}

// ---------------------------------------------------------------------------
// _UpdateScStatus - Smartcard-Statustext aktualisieren und UI benachrichtigen
// ---------------------------------------------------------------------------
void WireGuardCredential::_UpdateScStatus(PCWSTR pwszMsg)
{
    StringCchCopyW(_wszScStatus, MAX_LABEL_WGCP, pwszMsg);
    WCHAR d[MAX_LABEL_WGCP + 16] = {};
    StringCchPrintfW(d, ARRAYSIZE(d), L"SC-Status: %s", pwszMsg);
    LOG_DEBUG(d);
    if (_pCredProvCredentialEvents)
        _pCredProvCredentialEvents->SetFieldString(this, FI_SC_STATUS, _wszScStatus);
}

// ---------------------------------------------------------------------------
// _DoSmartcardAuth - Authentifizierung durchführen
// ---------------------------------------------------------------------------
bool WireGuardCredential::_DoSmartcardAuth()
{
    if (!_scConfig.bEnabled) return true;

    _UpdateScStatus(L"\U0001F50D Suche Smartcard...");
    LOG_DEBUG(L"SC: Starte Authentifizierung");

    // PIN-Länge loggen (nicht die PIN selbst)
    if (_scConfig.bPinRequired)
    {
        WCHAR d[64] = {};
        StringCchPrintfW(d, 64, L"SC: PIN-Laenge: %zu Zeichen", wcslen(_wszPin));
        LOG_DEBUG(d);
    }

    WGCPScResult result = WGCPAuthenticateSmartcard(_scConfig, _wszPin);

    // PIN sofort sicher löschen
    SecureZeroMemory(_wszPin, sizeof(_wszPin));

    switch (result)
    {
    case WGCPScResult::Success:
        StringCchCopyW(_wszCurrentReader, ARRAYSIZE(_wszCurrentReader),
                       _scConfig.wszReaderName[0] ? _scConfig.wszReaderName : L"");
        _UpdateScStatus(L"\u2705 Smartcard-Authentifizierung erfolgreich");
        _dwPinAttempts = 0;
        LOG_DEBUG(L"Smartcard: Authentifizierung erfolgreich");
        return true;

    case WGCPScResult::Timeout:
    case WGCPScResult::NoCard:
        _UpdateScStatus(L"\u26A0 Keine Smartcard / YubiKey gefunden");
        return false;

    case WGCPScResult::WrongCard:
        _UpdateScStatus(L"\u274C Falsche Smartcard (Thumbprint stimmt nicht)");
        return false;

    case WGCPScResult::PinWrong:
        _dwPinAttempts++;
        {
            WCHAR wszMsg[128] = {};
            DWORD dwRem = (_scConfig.dwPinMaxAttempts > _dwPinAttempts)
                          ? _scConfig.dwPinMaxAttempts - _dwPinAttempts : 0;
            StringCchPrintfW(wszMsg, ARRAYSIZE(wszMsg),
                             L"\u274C PIN falsch. Verbleibende Versuche: %lu", dwRem);
            _UpdateScStatus(wszMsg);
        }
        return false;

    case WGCPScResult::PinLocked:
        _UpdateScStatus(L"\U0001F512 PIN gesperrt. Bitte PIN mit YubiKey Manager entsperren.");
        return false;

    case WGCPScResult::Disabled:
        LOG_DEBUG(L"SC: Smartcard deaktiviert - Auth uebersprungen");
        return true;

    default:
        _UpdateScStatus(L"\u274C Smartcard-Fehler aufgetreten");
        return false;
    }
}

// ---------------------------------------------------------------------------
// Smartcard-Watch-Thread: überwacht Einstecken/Entfernen
// ---------------------------------------------------------------------------
DWORD WINAPI WireGuardCredential::_ScWatchThreadProc(LPVOID lpParam)
{
    WireGuardCredential* pThis = static_cast<WireGuardCredential*>(lpParam);
    bool bCardWasPresent = false;

    LOG_DEBUG(L"SC-Watch: Thread gestartet");

    while (!pThis->_bStopScWatch)
    {
        WCHAR wszReader[256] = {};
        bool bCardNow = WGCPFindSmartcard(pThis->_scConfig, wszReader, 256);

        if (bCardNow && !bCardWasPresent)
        {
            // Karte wurde eingesteckt
            StringCchCopyW(pThis->_wszCurrentReader, 256, wszReader);
            LOG_DEBUG(L"SC-Watch: Karte eingesteckt");

            if (pThis->_scConfig.bConnectOnInsert &&
                !pThis->_bConnected && pThis->_nProfiles > 0)
            {
                pThis->_UpdateScStatus(L"\U0001F511 Smartcard erkannt – authentifiziere...");
                if (pThis->_DoSmartcardAuth())
                {
                    PCWSTR pwszProfile = pThis->_rgProfiles[pThis->_dwSelectedProfile];
                    WCHAR d[MAX_PATH_WGCP + 32] = {};
                    StringCchPrintfW(d, ARRAYSIZE(d), L"SC-Watch: Auto-Connect Tunnel '%s'", pwszProfile);
                    LOG_DEBUG(d);
                    WGConnect(pThis->_wszExePath, pwszProfile);
                    for (int i = 0; i < 12; i++)
                    {
                        Sleep(500);
                        if (WGIsTunnelConnected(pwszProfile)) break;
                    }
                    pThis->_RefreshStatus();
                    pThis->_UpdateFields();
                    if (pThis->_pProvider)
                        pThis->_pProvider->NotifyStatusChanged();
                }
            }
            else
            {
                pThis->_UpdateScStatus(L"\u2705 Smartcard erkannt – PIN eingeben und Verbinden drücken");
            }
        }
        else if (!bCardNow && bCardWasPresent)
        {
            // Karte wurde entfernt
            LOG_DEBUG(L"SC-Watch: Karte entfernt");
            pThis->_UpdateScStatus(L"\U0001F511 Bitte YubiKey / Smartcard einstecken...");

            if (pThis->_scConfig.bDisconnectOnRemove && pThis->_bConnected)
            {
                PCWSTR pwszProfile = pThis->_rgProfiles[pThis->_dwSelectedProfile];
                LOG_DEBUG(L"SC-Watch: Auto-Disconnect wegen Karte entfernt");
                WGDisconnect(pThis->_wszExePath, pwszProfile);
                for (int i = 0; i < 12; i++)
                {
                    Sleep(500);
                    if (!WGIsTunnelConnected(pwszProfile)) break;
                }
                pThis->_RefreshStatus();
                pThis->_UpdateFields();
                if (pThis->_pProvider)
                    pThis->_pProvider->NotifyStatusChanged();
            }
        }

        bCardWasPresent = bCardNow;
        Sleep(1000);  // jede Sekunde prüfen
    }

    LOG_DEBUG(L"SC-Watch: Thread beendet");
    return 0;
}

// ---------------------------------------------------------------------------
// CommandLinkClicked - Verbinden / Trennen (mit Smartcard-Auth)
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
        // Smartcard-Authentifizierung wenn aktiviert
        if (_scConfig.bEnabled)
        {
            if (!_DoSmartcardAuth())
            {
                _RefreshStatus();
                _UpdateFields();
                return S_OK;
            }
        }

        WCHAR d[MAX_PATH_WGCP+32]={};
        StringCchPrintfW(d,ARRAYSIZE(d),L"Verbinde Tunnel: %s",pwszProfile); LOG_DEBUG(d);
        WGConnect(_wszExePath, pwszProfile);

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
