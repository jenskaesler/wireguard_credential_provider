#include "WireGuardProvider.h"
#include "WireGuardCredential.h"
#include "FieldDescriptors.h"
#include <new>

WireGuardProvider::WireGuardProvider()
    : _cRef(1), _pCredential(nullptr)
    , _bRecreateEnumeratedCredentials(true), _cpus(CPUS_INVALID)
    , _pcpe(nullptr), _upAdviseContext(0) {}

WireGuardProvider::~WireGuardProvider()
{
    if (_pCredential) { _pCredential->Release(); _pCredential=nullptr; }
    if (_pcpe) { _pcpe->Release(); _pcpe=nullptr; }
}

STDMETHODIMP WireGuardProvider::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] = { QITABENT(WireGuardProvider, ICredentialProvider), {0} };
    return QISearch(this, qit, riid, ppv);
}
STDMETHODIMP_(ULONG) WireGuardProvider::Release()
{
    LONG c = InterlockedDecrement(&_cRef); if (!c) delete this; return c;
}
STDMETHODIMP WireGuardProvider::SetUsageScenario(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus, DWORD)
{
    switch (cpus)
    {
    case CPUS_LOGON:
    case CPUS_UNLOCK_WORKSTATION:
        _cpus=cpus; _bRecreateEnumeratedCredentials=true; return S_OK;
    default:
        return E_NOTIMPL;
    }
}
STDMETHODIMP WireGuardProvider::SetSerialization(
    const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION*) { return E_NOTIMPL; }
STDMETHODIMP WireGuardProvider::Advise(ICredentialProviderEvents* pcpe, UINT_PTR upAdviseContext)
{
    if (_pcpe) { _pcpe->Release(); _pcpe = nullptr; }
    _pcpe = pcpe;
    if (_pcpe) _pcpe->AddRef();
    _upAdviseContext = upAdviseContext;
    return S_OK;
}
STDMETHODIMP WireGuardProvider::UnAdvise()
{
    if (_pcpe) { _pcpe->Release(); _pcpe = nullptr; }
    _upAdviseContext = 0;
    return S_OK;
}
STDMETHODIMP WireGuardProvider::GetFieldDescriptorCount(DWORD* pdwCount)
    { *pdwCount=FI_NUM_FIELDS; return S_OK; }

STDMETHODIMP WireGuardProvider::GetFieldDescriptorAt(
    DWORD dwIndex, CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** ppcpfd)
{
    *ppcpfd=nullptr;
    if (dwIndex>=FI_NUM_FIELDS) return E_INVALIDARG;
    *ppcpfd=static_cast<CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR*>(
        CoTaskMemAlloc(sizeof(CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR)));
    if (!*ppcpfd) return E_OUTOFMEMORY;
    **ppcpfd=g_rgFields[dwIndex];
    (*ppcpfd)->pszLabel=nullptr;
    HRESULT hr=S_OK;
    if (g_rgFields[dwIndex].pszLabel)
        hr=WGCPStrDup(g_rgFields[dwIndex].pszLabel,&(*ppcpfd)->pszLabel);
    if (FAILED(hr)) { CoTaskMemFree(*ppcpfd); *ppcpfd=nullptr; }
    return hr;
}

STDMETHODIMP WireGuardProvider::GetCredentialCount(
    DWORD* pdwCount, DWORD* pdwDefault, BOOL* pbAutoLogonWithDefault)
{
    if (_bRecreateEnumeratedCredentials)
        { _bRecreateEnumeratedCredentials=false; _EnumerateCredentials(); }
    *pdwCount=1; *pdwDefault=CREDENTIAL_PROVIDER_NO_DEFAULT;
    *pbAutoLogonWithDefault=FALSE; return S_OK;
}

STDMETHODIMP WireGuardProvider::GetCredentialAt(
    DWORD dwIndex, ICredentialProviderCredential** ppcpc)
{
    *ppcpc=nullptr;
    if (dwIndex!=0||!_pCredential) return E_INVALIDARG;
    return _pCredential->QueryInterface(
        IID_ICredentialProviderCredential, reinterpret_cast<void**>(ppcpc));
}

HRESULT WireGuardProvider::_EnumerateCredentials()
{
    if (_pCredential) { _pCredential->Release(); _pCredential=nullptr; }
    WireGuardCredential* p=new(std::nothrow) WireGuardCredential();
    if (!p) return E_OUTOFMEMORY;
    p->_pProvider = this;  // back-pointer
    HRESULT hr=p->Initialize(_cpus,g_rgFields,g_rgFieldStates);
    if (SUCCEEDED(hr)) _pCredential=p; else p->Release();
    return hr;
}

HRESULT WireGuardProvider_CreateInstance(REFIID riid, void** ppv)
{
    WireGuardProvider* p=new(std::nothrow) WireGuardProvider();
    if (!p) return E_OUTOFMEMORY;
    HRESULT hr=p->QueryInterface(riid,ppv);
    p->Release();
    return hr;
}
