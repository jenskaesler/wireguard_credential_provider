//
// main.cpp  –  Entry point for WireGuardCPTray.exe
// Part of the WireGuard Credential Provider project.
//
// Single-instance guard via named mutex.
//

#include "WireGuardTray.h"   // pulls in windows.h + <new> already

// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    // Single-instance guard
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, WGCP_TRAY_MUTEX);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // Wait for the shell taskbar to be ready (critical for autostart via Run key).
    // Shell_NotifyIconW silently fails if the taskbar is not yet initialized.
    HWND hTray = nullptr;
    for (int i = 0; i < 30 && !hTray; i++)
    {
        hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (!hTray) Sleep(1000);
    }
    // Extra 1 second buffer so the taskbar is ready to accept tray icons
    if (hTray) Sleep(1000);

    // Read Windows dark mode preference from registry
    bool bDarkMode = false;
    {
        HKEY hKey = nullptr;
        DWORD dwLight = 1, dwSize = sizeof(dwLight);
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&dwLight), &dwSize);
            RegCloseKey(hKey);
        }
        bDarkMode = (dwLight == 0); // 0 = dark mode active
    }

    // Configure UxTheme app mode so TrackPopupMenu respects the system theme
    HMODULE hUxTheme = LoadLibraryW(L"uxtheme.dll");
    if (hUxTheme)
    {
        // Ordinal 135 = SetPreferredAppMode (0=Default,1=AllowDark,2=ForceDark,3=ForceLight)
        typedef int (WINAPI* fnSetPreferredAppMode)(int);
        auto pfnMode = reinterpret_cast<fnSetPreferredAppMode>(
            GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135)));
        if (pfnMode) pfnMode(bDarkMode ? 2 : 3); // 2=ForceDark, 3=ForceLight

        // Ordinal 136 = FlushMenuThemes – clears the theme cache
        typedef void (WINAPI* fnFlushMenuThemes)();
        auto pfnFlush = reinterpret_cast<fnFlushMenuThemes>(
            GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136)));
        if (pfnFlush) pfnFlush();

        // Ordinal 133 = AllowDarkModeForApp (older API, kept for compatibility)
        typedef bool (WINAPI* fnAllowDarkModeForApp)(bool);
        auto pfnAllow = reinterpret_cast<fnAllowDarkModeForApp>(
            GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133)));
        if (pfnAllow) pfnAllow(bDarkMode);

        FreeLibrary(hUxTheme);
    }

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    WireGuardTrayApp* pApp = new(std::nothrow) WireGuardTrayApp();
    if (!pApp)
    {
        if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
        return 1;
    }

    int nRet = 1;
    if (pApp->Init(hInst))
        nRet = pApp->Run();

    delete pApp;
    if (hMutex) { ReleaseMutex(hMutex); CloseHandle(hMutex); }
    return nRet;
}
