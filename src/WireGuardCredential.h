#pragma once
//
// WireGuardCredential.h
//
// Implementierung von ICredentialProviderCredential.
// Repräsentiert die einzelne Kachel auf dem Windows-Anmeldebildschirm
// und steuert die gesamte WireGuard-Tunnel-Logik.
//

#include "helpers.h"

// ---------------------------------------------------------------------------
// Feld-IDs (Reihenfolge entspricht g_rgFields in FieldDescriptors.h)
// ---------------------------------------------------------------------------
enum FIELD_ID
{
    FI_TILEIMAGE  = 0,  // Kachel-Icon (wechselt je nach Verbindungsstatus)
    FI_LABEL      = 1,  // Überschrift ("WireGuard VPN")
    FI_STATUS     = 2,  // Verbindungsstatus mit Timer
    FI_TRAFFIC    = 3,  // Datendurchsatz (nur bei aktiver Verbindung)
    FI_PROFILE    = 4,  // Profil-Auswahl (ComboBox)
    FI_BUTTON     = 5,  // Aktion-Button ("Verbinden" / "Trennen")
    FI_NUM_FIELDS = 6
};

class WireGuardProvider;

class WireGuardCredential : public ICredentialProviderCredential
{
public:
    STDMETHODIMP_(ULONG) AddRef()  { return InterlockedIncrement(&_cRef); }
    STDMETHODIMP_(ULONG) Release();
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv);

    // ICredentialProviderCredential
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

    // WireGuard-Profile
    WCHAR   _rgProfiles[MAX_PROFILES][MAX_PATH_WGCP];
    int     _nProfiles;
    DWORD   _dwSelectedProfile;

    // Verbindungsstatus (volatile: wird aus Timer-Thread gelesen)
    volatile bool _bConnected;
    volatile bool _bSelected;

    // Konfigurationspfade
    WCHAR   _wszExePath[MAX_PATH_WGCP];
    WCHAR   _wszWgExePath[MAX_PATH_WGCP];
    WCHAR   _wszIconConn[MAX_PATH_WGCP];
    WCHAR   _wszIconDisconn[MAX_PATH_WGCP];

    // Anzeigetexte (gecacht, werden in _RefreshStatus aktualisiert)
    WCHAR   _wszStatus[MAX_LABEL_WGCP];
    WCHAR   _wszTraffic[MAX_LABEL_WGCP];

    // Hintergrund-Thread für automatische Status-Aktualisierung
    HANDLE          _hTimerThread;
    volatile bool   _bStopTimer;

    // Hintergrund-Thread für Shutdown-Erkennung
    HANDLE  _hShutdownThread;
    HWND    _hShutdownWnd;

    static DWORD WINAPI _TimerThreadProc(LPVOID lpParam);
    static DWORD WINAPI _ShutdownThreadProc(LPVOID lpParam);
    static LRESULT CALLBACK _ShutdownWndProc(HWND hWnd, UINT uMsg,
                                              WPARAM wParam, LPARAM lParam);

    void    _LoadConfig();
    void    _LoadProfiles();
    void    _DisconnectAllOnBoot();
    void    _RefreshStatus();
    void    _UpdateFields();
    HRESULT _LoadBitmap(PCWSTR pwszPath, HBITMAP* phbmp);
};
