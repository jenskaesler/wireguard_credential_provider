#include "WireGuardCredential.h"
#include "WireGuardProvider.h"
#include "FieldDescriptors.h"
#include <new>

// ---------------------------------------------------------------------------
// Field definitions - NO 4-byte emoji, only BMP Unicode (max \uFFFF)
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
    _dwHandshakeTimeoutSec = 0;
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
    // Stop timer thread
    _bStopTimer = true;
    if (_hTimerThread)
    {
        WaitForSingleObject(_hTimerThread, 3000);
        CloseHandle(_hTimerThread);
        _hTimerThread = nullptr;
    }
    // Stop smartcard watch thread
    _bStopScWatch = true;
    if (_hScWatchThread)
    {
        WaitForSingleObject(_hScWatchThread, 3000);
        CloseHandle(_hScWatchThread);
        _hScWatchThread = nullptr;
    }
    // Securely erase PIN
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

        // Load smartcard configuration
WGCPLoadSmartcardConfig(_scConfig);
        if (_scConfig.bEnabled)
        {
            LOG_DEBUG(L"Smartcard authentication enabled");
            // Make SC status field visible
            _rgFieldStatePairs[FI_SC_STATUS].cpfs = CPFS_DISPLAY_IN_SELECTED_TILE;
            if (_scConfig.bPinRequired)
                _rgFieldStatePairs[FI_PIN].cpfs = CPFS_DISPLAY_IN_SELECTED_TILE;
            StringCchCopyW(_wszScStatus, MAX_LABEL_WGCP,
                           L"\U0001F511 Please insert your YubiKey...");
        }

        // Log rotation
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

        // Start smartcard watch thread (connect/disconnect on insert/remove)
        if (_scConfig.bEnabled &&
            (_scConfig.bConnectOnInsert || _scConfig.bDisconnectOnRemove))
        {
            _hScWatchThread = CreateThread(nullptr, 0, _ScWatchThreadProc, this, 0, nullptr);
            if (_hScWatchThread)
                LOG_DEBUG(L"Smartcard watch thread started");
        }
    }
    else
    {
        WCHAR e[64]={};
        StringCchPrintfW(e, 64, L"Initialize failed: 0x%08X", hr);
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
        _dwHandshakeTimeoutSec = ReadRegDword(hKey, WGCP_REG_HANDSHAKE_TIMEOUT_SEC, 0);
        RegCloseKey(hKey);
        LOG_DEBUG(L"Registry config loaded");
    }
    else
    {
        LOG_WARN(L"Registry key missing, using defaults");
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
    StringCchPrintfW(d, 128, L"%d profile(s) found in directory", _nProfiles);
    LOG_DEBUG(d);

    for (int i = 0; i < _nProfiles; i++)
    {
        WCHAR d2[MAX_PATH_WGCP+16]={};
        StringCchPrintfW(d2, ARRAYSIZE(d2), L"  Profile[%d]: %s", i, _rgProfiles[i]);
        LOG_DEBUG(d2);
    }

    if (_nProfiles == 0) return;

    // Use computer name as default profile
    WCHAR wszComp[MAX_PATH_WGCP]={};
    DWORD dwSize = MAX_PATH_WGCP;
    GetComputerNameW(wszComp, &dwSize);

    WCHAR dc[MAX_PATH_WGCP+32]={};
    StringCchPrintfW(dc, ARRAYSIZE(dc), L"Computer name: %s", wszComp);
    LOG_DEBUG(dc);

    for (int i = 0; i < _nProfiles; i++)
    {
        if (_wcsicmp(_rgProfiles[i], wszComp) == 0)
        {
            _dwSelectedProfile = static_cast<DWORD>(i);
            WCHAR d2[MAX_PATH_WGCP+32]={};
            StringCchPrintfW(d2, ARRAYSIZE(d2), L"Default profile: %s (index %d)", wszComp, i);
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
        StringCchCopyW(_wszStatus,  MAX_LABEL_WGCP, L"\u25CB No profile");
        StringCchCopyW(_wszTraffic, MAX_LABEL_WGCP, L"");
        LOG_DEBUG(L"No profile available");
        return;
    }

    PCWSTR pwszProfile = _rgProfiles[_dwSelectedProfile];
    _bConnected = WGIsTunnelConnected(pwszProfile);

    if (_bConnected)
    {
        WCHAR wszTimer[MAX_LABEL_WGCP]={};
        WGGetConnectedSince(pwszProfile, wszTimer, MAX_LABEL_WGCP);
        StringCchCopyW(_wszStatus, MAX_LABEL_WGCP,
                       wszTimer[0] ? wszTimer : L"\u25CF Connected");

        WGGetTrafficStats(_wszWgExePath, pwszProfile, _wszTraffic, MAX_LABEL_WGCP);
    }
    else
    {
        StringCchCopyW(_wszStatus,  MAX_LABEL_WGCP, L"\u25CB Disconnected");
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
    PCWSTR pwszBtn = (_nProfiles == 0) ? L"No profile"
                   : (_bConnected      ? L"\u23CF  Disconnect"
                                       : L"\u25B6  Connect");
    _pCredProvCredentialEvents->SetFieldString(this, FI_BUTTON, pwszBtn);
    _pCredProvCredentialEvents->SetFieldInteractiveState(
        this, FI_BUTTON,
        (_nProfiles > 0) ? CPFIS_INTERACTIVE : CPFIS_NONE);
}

// ---------------------------------------------------------------------------
// _LoadBitmap
// Loads bitmap first from the embedded DLL resource.
// Fallback: file path from registry (for custom icons).
// ---------------------------------------------------------------------------

// Static helper function as anchor for GetModuleHandleExW
static void _WGCPAnchor() {}

HRESULT WireGuardCredential::_LoadBitmap(PCWSTR pwszPath, HBITMAP* phbmp)
{
    *phbmp = nullptr;

    // Ressource-ID je nach Status
    UINT uResId = _bConnected ? IDB_WIREGUARD_CONNECTED : IDB_WIREGUARD_DISCONNECTED;

    // Get DLL instance via static anchor function
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
        StringCchPrintfW(e, 64, L"Resource %u not found, error=%lu", uResId, GetLastError());
        LOG_WARN(e);
    }

    // Fallback: file from registry path
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
        StringCchPrintfW(e, ARRAYSIZE(e), L"LoadImage file err=%lu: %s", GetLastError(), pwszPath);
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
    LOG_DEBUG(L"Tile selected");
    *pbAutoLogon = FALSE;
    _bSelected = true;
    _RefreshStatus();
    _UpdateFields();

    // Start auto-refresh timer
    _bStopTimer = false;
    if (!_hTimerThread)
    {
        _hTimerThread = CreateThread(nullptr, 0, _TimerThreadProc, this, 0, nullptr);
        LOG_DEBUG(L"Auto-refresh timer started");
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
        LOG_DEBUG(L"Auto-refresh timer stopped");
    }
    return S_OK;
}


// ---------------------------------------------------------------------------
// Timer thread: update status and traffic every 5 seconds
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
        LOG_DEBUG(L"Auto-refresh: status updated");
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
    // PIN field: visible when disconnected + smartcard enabled + PIN required
    if (dwFieldID == FI_PIN)
    {
        *pcpfs  = (!_bConnected && _scConfig.bEnabled && _scConfig.bPinRequired)
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
    case FI_PIN:
        // Return empty string to visually mask PIN input.
        // The actual PIN is stored in _wszPin via SetStringValue.
        return WGCPStrDup(L"", ppwsz);
    case FI_BUTTON:
    {
        PCWSTR p = (_nProfiles == 0) ? L"No profile"
                 : (_bConnected      ? L"\u23CF  Disconnect"
                                     : L"\u25B6  Connect");
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
// _UpdateScStatus - update smartcard status text and notify UI
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
// _DoSmartcardAuth - perform smartcard authentication
// ---------------------------------------------------------------------------
bool WireGuardCredential::_DoSmartcardAuth()
{
    if (!_scConfig.bEnabled) return true;

    _UpdateScStatus(L"\U0001F50D Searching for smartcard...");
LOG_DEBUG(L"SC: Starting authentication");

    // Log thumbprint that will be used for verification
    if (_scConfig.wszCertThumbprint[0])
    {
        WCHAR dT[160] = {};
        StringCchPrintfW(dT, 160, L"SC: Using thumbprint: '%s'",
                         _scConfig.wszCertThumbprint);
        LOG_DEBUG(dT);
    }
    else
    {
        LOG_WARN(L"SC: No thumbprint configured - check registry SmartcardCertThumbprint");
    }

    // Log PIN length (never the PIN itself)
    if (_scConfig.bPinRequired)
    {
        WCHAR d[64] = {};
        StringCchPrintfW(d, 64, L"SC: PIN length: %zu characters", wcslen(_wszPin));
        LOG_DEBUG(d);
    }

    WGCPScResult result = WGCPAuthenticateSmartcard(_scConfig, _wszPin);
    {
        char szR[64]={};
        wsprintfA(szR,"DoSmartcardAuth: result=%d",(int)result);
        }

    // Immediately erase PIN from memory
    SecureZeroMemory(_wszPin, sizeof(_wszPin));

    switch (result)
    {
    case WGCPScResult::Success:
        StringCchCopyW(_wszCurrentReader, ARRAYSIZE(_wszCurrentReader),
                       _scConfig.wszReaderName[0] ? _scConfig.wszReaderName : L"");
        _UpdateScStatus(L"\u2705 Smartcard authentication successful");
        _dwPinAttempts = 0;
        LOG_DEBUG(L"Smartcard: Authentication successful");
        return true;

    case WGCPScResult::Timeout:
    case WGCPScResult::NoCard:
        _UpdateScStatus(L"\u26A0 No smartcard / YubiKey found");
        return false;

    case WGCPScResult::WrongCard:
        _UpdateScStatus(L"\u274C Wrong YubiKey (thumbprint mismatch)");
        return false;

    case WGCPScResult::PinWrong:
        _dwPinAttempts++;
        {
            WCHAR wszMsg[128] = {};
            DWORD dwRem = (_scConfig.dwPinMaxAttempts > _dwPinAttempts)
                          ? _scConfig.dwPinMaxAttempts - _dwPinAttempts : 0;
            StringCchPrintfW(wszMsg, ARRAYSIZE(wszMsg),
                             L"\u274C Wrong PIN. Remaining attempts: %lu", dwRem);
            _UpdateScStatus(wszMsg);
        }
        return false;

    case WGCPScResult::PinLocked:
        _UpdateScStatus(L"\U0001F512 PIN locked. Please unlock PIN with YubiKey Manager.");
        return false;

    case WGCPScResult::Disabled:
        LOG_DEBUG(L"SC: Smartcard disabled - auth skipped");
        return true;

    default:
        _UpdateScStatus(L"\u274C Smartcard error occurred");
        return false;
    }
}

// ---------------------------------------------------------------------------
// Smartcard watch thread: monitors card insert/remove events
// ---------------------------------------------------------------------------
DWORD WINAPI WireGuardCredential::_ScWatchThreadProc(LPVOID lpParam)
{
    WireGuardCredential* pThis = static_cast<WireGuardCredential*>(lpParam);
    LOG_DEBUG(L"SC-Watch: Thread started");

    // Check initial card state - card may already be present at startup
    WCHAR wszInitReader[256] = {};
    bool bCardWasPresent = WGCPFindSmartcard(pThis->_scConfig, wszInitReader, 256);
    if (bCardWasPresent)
    {
        StringCchCopyW(pThis->_wszCurrentReader, 256, wszInitReader);
        if (pThis->_bConnected)
            pThis->_UpdateScStatus(L"\U0001F512 Connection secured by YubiKey");
        else if (pThis->_scConfig.bPinRequired)
            pThis->_UpdateScStatus(L"\u2705 YubiKey detected \u2013 enter PIN and click Connect");
        else
            pThis->_UpdateScStatus(L"\u2705 YubiKey detected \u2013 click Connect");
        LOG_DEBUG(L"SC-Watch: Card already present at startup");
    }

    while (!pThis->_bStopScWatch)
    {
        WCHAR wszReader[256] = {};
        bool bCardNow = WGCPFindSmartcard(pThis->_scConfig, wszReader, 256);

        if (bCardNow && !bCardWasPresent)
        {
            // Karte wurde eingesteckt
            StringCchCopyW(pThis->_wszCurrentReader, 256, wszReader);
            LOG_DEBUG(L"SC-Watch: Card inserted");

            if (pThis->_scConfig.bConnectOnInsert &&
                !pThis->_bConnected && pThis->_nProfiles > 0)
            {
                pThis->_UpdateScStatus(L"\U0001F511 YubiKey detected \u2013 authenticating...");
                if (pThis->_DoSmartcardAuth())
                {
                    PCWSTR pwszProfile = pThis->_rgProfiles[pThis->_dwSelectedProfile];
                    WCHAR d[MAX_PATH_WGCP + 32] = {};
                    StringCchPrintfW(d, ARRAYSIZE(d), L"SC-Watch: Auto-connect tunnel '%s'", pwszProfile);
                    LOG_DEBUG(d);
                    WGConnect(pThis->_wszExePath, pwszProfile);
                    for (int i = 0; i < 12; i++)
                    {
                        Sleep(500);
                        if (WGIsTunnelConnected(pwszProfile)) break;
                    }
                    pThis->_RefreshStatus();
                    pThis->_UpdateFields();
                    if (pThis->_bConnected)
                        pThis->_UpdateScStatus(L"\U0001F512 Connection secured by YubiKey");
                    if (pThis->_pProvider)
                        pThis->_pProvider->NotifyStatusChanged();
                }
            }
            else
            {
                if (pThis->_bConnected)
                    pThis->_UpdateScStatus(L"\U0001F512 Connection secured by YubiKey");
                else if (pThis->_scConfig.bPinRequired)
                    pThis->_UpdateScStatus(L"\u2705 YubiKey detected \u2013 enter PIN and click Connect");
                else
                    pThis->_UpdateScStatus(L"\u2705 YubiKey detected \u2013 click Connect");
            }
        }
        else if (!bCardNow && bCardWasPresent)
        {
            // Karte wurde entfernt
            LOG_DEBUG(L"SC-Watch: Card removed");
            pThis->_UpdateScStatus(L"\U0001F511 Please insert your YubiKey...");

            if (pThis->_scConfig.bDisconnectOnRemove && pThis->_bConnected)
            {
                PCWSTR pwszProfile = pThis->_rgProfiles[pThis->_dwSelectedProfile];
                LOG_DEBUG(L"SC-Watch: Auto-disconnect due to card removal");
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

        // Handshake timeout: disconnect if last handshake is too old
        if (pThis->_bConnected && pThis->_dwHandshakeTimeoutSec > 0)
        {
            PCWSTR pwszProf = pThis->_rgProfiles[pThis->_dwSelectedProfile];
            LONGLONG llAge = WGGetLastHandshakeSec(pThis->_wszWgExePath, pwszProf);
            if (llAge > static_cast<LONGLONG>(pThis->_dwHandshakeTimeoutSec))
            {
                WCHAR d[128] = {};
                StringCchPrintfW(d, 128,
                    L"Handshake timeout: last handshake %lld s ago (limit %lu s) - disconnecting tunnel",
                    llAge, pThis->_dwHandshakeTimeoutSec);
                LOG_CRIT(d);
                WGDisconnect(pThis->_wszExePath, pwszProf);
            }
        }

        // Update status text on every tick when card is present
        // (handles case where _bConnected changes while card stays inserted)
        if (bCardNow)
        {
            if (pThis->_bConnected)
                pThis->_UpdateScStatus(L"\U0001F512 Connection secured by YubiKey");
            else if (pThis->_scConfig.bPinRequired)
                pThis->_UpdateScStatus(L"\u2705 YubiKey detected \u2013 enter PIN and click Connect");
            else
                pThis->_UpdateScStatus(L"\u2705 YubiKey detected \u2013 click Connect");
        }

        bCardWasPresent = bCardNow;
        Sleep(1000);  // check every second
    }

    LOG_DEBUG(L"SC-Watch: Thread stopped");
    return 0;
}

// ---------------------------------------------------------------------------
// CommandLinkClicked - connect / disconnect (with optional smartcard auth)
// ---------------------------------------------------------------------------
STDMETHODIMP WireGuardCredential::CommandLinkClicked(DWORD dwFieldID)
{
if (dwFieldID != FI_BUTTON || _nProfiles == 0) return S_OK;

    PCWSTR pwszProfile = _rgProfiles[_dwSelectedProfile];

    if (_bConnected)
    {
        WCHAR d[MAX_PATH_WGCP+32]={};
        StringCchPrintfW(d,ARRAYSIZE(d),L"Disconnecting tunnel: %s",pwszProfile); LOG_DEBUG(d);
        WGDisconnect(_wszExePath, pwszProfile);

        for (int i = 0; i < 12; i++)
        {
            Sleep(500);
            if (!WGIsTunnelConnected(pwszProfile))
            {
                LOG_DEBUG(L"Tunnel service stopped");
                break;
            }
        }
    }
    else
    {
        // Block VPN connect when already on the corporate network
        if (WGCPIsOnCorporateNetwork())
        {
            LOG_DEBUG(L"CLC: Corporate network detected - connect blocked");
            _UpdateScStatus(
                L"\U0001F3E2 Corporate network \u2013 VPN not needed");
            _UpdateFields();
            return S_OK;
        }

        // Smartcard authentication if enabled
        if (_scConfig.bEnabled)
        {
            if (!_DoSmartcardAuth())
            {
                SecureZeroMemory(_wszPin, sizeof(_wszPin));
                _RefreshStatus();
                _UpdateFields();
                return S_OK;
            }
            SecureZeroMemory(_wszPin, sizeof(_wszPin));
        }

        WCHAR d[MAX_PATH_WGCP+32]={};
        StringCchPrintfW(d,ARRAYSIZE(d),L"Connecting tunnel: %s",pwszProfile); LOG_DEBUG(d);
        WGConnect(_wszExePath, pwszProfile);

        for (int i = 0; i < 12; i++)
        {
            Sleep(500);
            if (WGIsTunnelConnected(pwszProfile))
            {
                LOG_DEBUG(L"Tunnel service started");
                break;
            }
        }

    }
    _RefreshStatus();
    _UpdateFields();
    // Icon reload: CredentialsChanged only after an actual connect/disconnect action
    if (_pProvider) _pProvider->NotifyStatusChanged();
    return S_OK;
}

// ---------------------------------------------------------------------------
STDMETHODIMP WireGuardCredential::GetCheckboxValue(DWORD, BOOL*, WCHAR**) { return E_NOTIMPL; }
STDMETHODIMP WireGuardCredential::GetSubmitButtonValue(DWORD, DWORD*)     { return E_NOTIMPL; }
STDMETHODIMP WireGuardCredential::SetCheckboxValue(DWORD, BOOL)           { return E_NOTIMPL; }


STDMETHODIMP WireGuardCredential::GetSerialization(
    CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr,
    CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs,
    WCHAR** ppwszOptionalStatusText,
    CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon)
{
    *ppwszOptionalStatusText = nullptr;
    *pcpsiOptionalStatusIcon = CPSI_NONE;

    LOG_DEBUG(L"CP: GetSerialization called");

    // Extract PIN from serialization buffer if PIN is required
    // Windows passes CPFT_PASSWORD_TEXT values here, not via SetStringValue
    if (_scConfig.bEnabled && _scConfig.bPinRequired && pcpcs && pcpcs->cbSerialization > 0)
    {
        // Try to unpack credentials to get the password field
        WCHAR wszUser[256] = {};
        WCHAR wszPass[64]  = {};
        WCHAR wszDomain[64] = {};
        DWORD cchUser   = ARRAYSIZE(wszUser);
        DWORD cchPass   = ARRAYSIZE(wszPass);
        DWORD cchDomain = ARRAYSIZE(wszDomain);
        if (CredUnPackAuthenticationBufferW(
                CRED_PACK_PROTECTED_CREDENTIALS,
                pcpcs->rgbSerialization, pcpcs->cbSerialization,
                wszUser, &cchUser,
                wszDomain, &cchDomain,
                wszPass, &cchPass) && wszPass[0])
        {
            StringCchCopyW(_wszPin, ARRAYSIZE(_wszPin), wszPass);
            SecureZeroMemory(wszPass, sizeof(wszPass));
            WCHAR d[64] = {};
            StringCchPrintfW(d, 64, L"SC: PIN extracted from serialization len=%zu",
                             wcslen(_wszPin));
            LOG_DEBUG(d);
        }
        else
        {
            LOG_WARN(L"SC: CredUnPackAuthenticationBuffer failed or empty password");
        }
    }

    // Trigger connect if not already connected
    if (!_bConnected && _scConfig.bEnabled && _nProfiles > 0)
    {
        LOG_DEBUG(L"CP: Triggering connect from GetSerialization");
        CommandLinkClicked(FI_BUTTON);
    }

    *pcpgsr = CPGSR_NO_CREDENTIAL_NOT_FINISHED;
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
