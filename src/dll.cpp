#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#include <shlwapi.h>
#include "WireGuardProvider.h"
#include <new>

static LONG      g_cRef  = 0;
static HINSTANCE g_hInst = nullptr;

void DllAddRef()  { InterlockedIncrement(&g_cRef); }
void DllRelease() { InterlockedDecrement(&g_cRef); }

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hInst;
        DisableThreadLibraryCalls(hInst);
    }
    return TRUE;
}

class CClassFactory : public IClassFactory
{
public:
    CClassFactory() : _cRef(1) { DllAddRef(); }

    STDMETHODIMP_(ULONG) AddRef()  { return InterlockedIncrement(&_cRef); }
    STDMETHODIMP_(ULONG) Release()
    {
        LONG c = InterlockedDecrement(&_cRef);
        if (!c) { DllRelease(); delete this; }
        return c;
    }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv)
    {
        static const QITAB qit[] = { QITABENT(CClassFactory, IClassFactory), { 0 } };
        return QISearch(this, qit, riid, ppv);
    }
    STDMETHODIMP CreateInstance(IUnknown* pOuter, REFIID riid, void** ppv)
    {
        *ppv = nullptr;
        if (pOuter) return CLASS_E_NOAGGREGATION;
        return WireGuardProvider_CreateInstance(riid, ppv);
    }
    STDMETHODIMP LockServer(BOOL bLock)
    {
        if (bLock) DllAddRef(); else DllRelease();
        return S_OK;
    }
private:
    LONG _cRef;
};

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    *ppv = nullptr;
    if (!IsEqualCLSID(rclsid, CLSID_WireGuardProvider))
        return CLASS_E_CLASSNOTAVAILABLE;

    CClassFactory* pf = new(std::nothrow) CClassFactory();
    if (!pf) return E_OUTOFMEMORY;
    HRESULT hr = pf->QueryInterface(riid, ppv);
    pf->Release();
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return (g_cRef == 0) ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer()
{
    WCHAR wszCLSID[64]  = {};
    WCHAR wszPath[256]  = {};
    WCHAR wszDll[MAX_PATH] = {};
    HKEY  hKey = nullptr;

    if (!StringFromGUID2(CLSID_WireGuardProvider, wszCLSID, ARRAYSIZE(wszCLSID)))
        return E_FAIL;

    if (!GetModuleFileNameW(g_hInst, wszDll, ARRAYSIZE(wszDll)))
        return HRESULT_FROM_WIN32(GetLastError());

    // CLSID
    StringCchPrintfW(wszPath, ARRAYSIZE(wszPath),
                     L"SOFTWARE\\Classes\\CLSID\\%s", wszCLSID);
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, wszPath, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
                       (const BYTE*)L"WireGuard Credential Provider",
                       sizeof(L"WireGuard Credential Provider"));
        RegCloseKey(hKey);
    }

    // InprocServer32
    StringCchPrintfW(wszPath, ARRAYSIZE(wszPath),
                     L"SOFTWARE\\Classes\\CLSID\\%s\\InprocServer32", wszCLSID);
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, wszPath, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
                       (const BYTE*)wszDll,
                       (DWORD)((wcslen(wszDll) + 1) * sizeof(WCHAR)));
        RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ,
                       (const BYTE*)L"Apartment", sizeof(L"Apartment"));
        RegCloseKey(hKey);
    }

    // Credential Provider Eintrag
    StringCchPrintfW(wszPath, ARRAYSIZE(wszPath),
                     L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\"
                     L"Authentication\\Credential Providers\\%s", wszCLSID);
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, wszPath, 0, nullptr,
                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS)
    {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ,
                       (const BYTE*)L"WireGuard Credential Provider",
                       sizeof(L"WireGuard Credential Provider"));
        RegCloseKey(hKey);
    }

    return S_OK;
}

STDAPI DllUnregisterServer()
{
    WCHAR wszCLSID[64] = {};
    WCHAR wszPath[256] = {};

    if (!StringFromGUID2(CLSID_WireGuardProvider, wszCLSID, ARRAYSIZE(wszCLSID)))
        return E_FAIL;

    StringCchPrintfW(wszPath, ARRAYSIZE(wszPath),
                     L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\"
                     L"Authentication\\Credential Providers\\%s", wszCLSID);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, wszPath);

    StringCchPrintfW(wszPath, ARRAYSIZE(wszPath),
                     L"SOFTWARE\\Classes\\CLSID\\%s\\InprocServer32", wszCLSID);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, wszPath);

    StringCchPrintfW(wszPath, ARRAYSIZE(wszPath),
                     L"SOFTWARE\\Classes\\CLSID\\%s", wszCLSID);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, wszPath);

    return S_OK;
}
