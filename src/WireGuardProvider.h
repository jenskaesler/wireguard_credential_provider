#pragma once
//
// WireGuardProvider.h
//
// Implementierung von ICredentialProvider.
// Registriert den Credential Provider bei Windows und verwaltet
// die Lebenszeit des WireGuardCredential-Objekts.
//

#include "helpers.h"

class WireGuardCredential;

// CLSID des Credential Providers
// Muss mit dem Eintrag in DllRegisterServer übereinstimmen.
// {4A1B2C3D-4E5F-6789-ABCD-EF0123456789}
static const CLSID CLSID_WireGuardProvider =
{ 0x4A1B2C3D, 0x4E5F, 0x6789, { 0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89 } };

class WireGuardProvider : public ICredentialProvider
{
public:
    STDMETHODIMP_(ULONG) AddRef()  { return InterlockedIncrement(&_cRef); }
    STDMETHODIMP_(ULONG) Release();
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv);

    // ICredentialProvider
    STDMETHODIMP SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, DWORD dwFlags);
    STDMETHODIMP SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* pcpcs);
    STDMETHODIMP Advise(ICredentialProviderEvents* pcpe, UINT_PTR upAdviseContext);
    STDMETHODIMP UnAdvise();
    STDMETHODIMP GetFieldDescriptorCount(DWORD* pdwCount);
    STDMETHODIMP GetFieldDescriptorAt(DWORD dwIndex,
                                       CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd);
    STDMETHODIMP GetCredentialCount(DWORD* pdwCount, DWORD* pdwDefault,
                                    BOOL* pbAutoLogonWithDefault);
    STDMETHODIMP GetCredentialAt(DWORD dwIndex,
                                  ICredentialProviderCredential** ppcpc);

    // Benachrichtigung an LogonUI dass sich der Status geändert hat
    void NotifyStatusChanged()
    {
        if (_pcpe) _pcpe->CredentialsChanged(_upAdviseContext);
    }

    friend HRESULT WireGuardProvider_CreateInstance(REFIID riid, void** ppv);

protected:
    WireGuardProvider();
    ~WireGuardProvider();

private:
    LONG                                _cRef;
    WireGuardCredential*                _pCredential;
    bool                                _bRecreateEnumeratedCredentials;
    CREDENTIAL_PROVIDER_USAGE_SCENARIO  _cpus;
    ICredentialProviderEvents*          _pcpe;
    UINT_PTR                            _upAdviseContext;

    HRESULT _EnumerateCredentials();
};
