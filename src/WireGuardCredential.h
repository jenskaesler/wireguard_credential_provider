#pragma once
//
// WireGuardCredential.h
//
// Implementation of ICredentialProviderCredential.
// Represents the single tile on the Windows login screen
// and controls all WireGuard tunnel and smartcard logic.
//

#include "helpers.h"

// ---------------------------------------------------------------------------
// Field IDs (order matches g_rgFields in FieldDescriptors.h)
// ---------------------------------------------------------------------------
enum FIELD_ID
{
    FI_TILEIMAGE  = 0,  // Tile icon
    FI_LABEL      = 1,  // Heading
    FI_STATUS     = 2,  // Connection status with timer
    FI_TRAFFIC    = 3,  // Data throughput
    FI_PROFILE    = 4,  // Profile selection (ComboBox)
    FI_SC_STATUS  = 5,  // Smartcard status ("Please insert YubiKey...")
    FI_PIN        = 6,  // PIN input (visible only when SC active + PIN required)
    FI_BUTTON     = 7,  // Action button
    FI_NUM_FIELDS = 8
};

class WireGuardProvider;

class WireGuardCredential : public ICredentialProviderCredential
{
public:
    STDMETHODIMP_(ULONG) AddRef()  { return InterlockedIncrement(&_cRef); }
    STDMETHODIMP_(ULONG) Release();
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv);

    STDMETHODIMP Advise(ICredentialProviderCredentialEvents* pcpce);
    STDMETHODIMP UnAdvise();
    STDMETHODIMP SetSelected(BOOL* pbAutoLogon);
    STDMETHODIMP SetDeselected();
    STDMETHODIMP GetFieldState(DWORD dwFieldID,
                               CREDENTIAL_PROVIDER_FIELD_STATE* pcpfs,
                               CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* pcpfis);
    STDMETHODIMP GetStringValue(DWORD dwFieldID, WCHAR** ppwsz);
    STDMETHODIMP GetBitmapValue(DWORD dwFieldID, HBITMAP* phbmp);
    STDMETHODIMP GetCheckboxValue(DWORD dwFieldID, BOOL* pbChecked, WCHAR** ppwszLabel);
    STDMETHODIMP GetComboBoxValueCount(DWORD dwFieldID, DWORD* pcItems, DWORD* pdwSelectedItem);
    STDMETHODIMP GetComboBoxValueAt(DWORD dwFieldID, DWORD dwItem, WCHAR** ppwszItem);
    STDMETHODIMP GetSubmitButtonValue(DWORD dwFieldID, DWORD* pdwAdjacentTo);
    STDMETHODIMP SetStringValue(DWORD dwFieldID, PCWSTR pwz);
    STDMETHODIMP SetCheckboxValue(DWORD dwFieldID, BOOL bChecked);
    STDMETHODIMP SetComboBoxSelectedValue(DWORD dwFieldID, DWORD dwSelectedItem);
    STDMETHODIMP CommandLinkClicked(DWORD dwFieldID);
    STDMETHODIMP GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* pcpgsr,
                                  CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs,
                                  WCHAR** ppwszOptionalStatusText,
                                  CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon);
    STDMETHODIMP ReportResult(NTSTATUS ntsStatus, NTSTATUS ntsSubstatus,
                              WCHAR** ppwszOptionalStatusText,
                              CREDENTIAL_PROVIDER_STATUS_ICON* pcpsiOptionalStatusIcon);

    HRESULT Initialize(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,
                       const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR* rgcpfd,
                       const FIELD_STATE_PAIR* rgfsp);

    friend class WireGuardProvider;

protected:
    WireGuardCredential();
    ~WireGuardCredential();

private:
    LONG                                    _cRef;
    CREDENTIAL_PROVIDER_USAGE_SCENARIO      _cpus;
    CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR    _rgCredProvFieldDescriptors[FI_NUM_FIELDS];
    FIELD_STATE_PAIR                        _rgFieldStatePairs[FI_NUM_FIELDS];
    ICredentialProviderCredentialEvents*    _pCredProvCredentialEvents;
    WireGuardProvider*                      _pProvider;

    // WireGuard profiles
    WCHAR   _rgProfiles[MAX_PROFILES][MAX_PATH_WGCP];
    int     _nProfiles;
    DWORD   _dwSelectedProfile;

    // Connection status
    volatile bool _bConnected;
    volatile bool _bSelected;

    // Configuration paths
    WCHAR   _wszExePath[MAX_PATH_WGCP];
    WCHAR   _wszWgExePath[MAX_PATH_WGCP];
    DWORD   _dwHandshakeTimeoutSec;    // 0 = disabled
    WCHAR   _wszIconConn[MAX_PATH_WGCP];
    WCHAR   _wszIconDisconn[MAX_PATH_WGCP];

    // Display strings
    WCHAR   _wszStatus[MAX_LABEL_WGCP];
    WCHAR   _wszTraffic[MAX_LABEL_WGCP];

    // Smartcard configuration and state
    WGCPSmartcardConfig _scConfig;
    WCHAR   _wszPin[64];            // PIN input (erased after use)
    WCHAR   _wszScStatus[MAX_LABEL_WGCP]; // SC status display
    DWORD   _dwPinAttempts;         // Failed attempts since last success
    WCHAR   _wszCurrentReader[256]; // Reader in which a card was last detected

    // Auto-connect/disconnect threads
    HANDLE          _hTimerThread;
    volatile bool   _bStopTimer;
    HANDLE          _hScWatchThread;    // Monitors card insert/remove events
    volatile bool   _bStopScWatch;

    static DWORD WINAPI _TimerThreadProc(LPVOID lpParam);
    static DWORD WINAPI _ScWatchThreadProc(LPVOID lpParam);

    void    _LoadConfig();
    void    _LoadProfiles();
    void    _RefreshStatus();
    void    _UpdateFields();
    void    _UpdateScStatus(PCWSTR pwszMsg);
    bool    _DoSmartcardAuth();         // Performs auth, displays error messages
    HRESULT _LoadBitmap(PCWSTR pwszPath, HBITMAP* phbmp);
};
