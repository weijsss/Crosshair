#include <windows.h>
#include <gdiplus.h>
#include "config.h"
#include "overlay.h"
#include "settings.h"

#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

static ULONG_PTR g_gdiplusToken;

int main() {
    // Kill previous instance
    HWND prev = FindWindowW(L"CrosshairSettings", nullptr);
    if (prev) {
        SendMessage(prev, WM_CLOSE, 0, 0);
        Sleep(500);
    }

    // Init GDI+
    Gdiplus::GdiplusStartupInput gdiSI;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiSI, nullptr);

    // Init COM for file dialogs
    CoInitialize(nullptr);

    // Load config
    AppCfg cfg;
    cfg.load();

    // Create overlay windows
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    OverlayManager overlay;
    overlay.create(nullptr, sw, sh);
    overlay.set_force_topmost(true);
    overlay.update(cfg);

    // Create settings window
    HINSTANCE hinst = GetModuleHandle(nullptr);
    SettingsWindow settings;
    settings.create(hinst, cfg, overlay);
    settings.show();

    // Global hotkeys: Ctrl+Shift+F1/F2/F3 toggle layer visibility
    RegisterHotKey(nullptr, 1, MOD_CONTROL | MOD_SHIFT, VK_F1);
    RegisterHotKey(nullptr, 2, MOD_CONTROL | MOD_SHIFT, VK_F2);
    RegisterHotKey(nullptr, 3, MOD_CONTROL | MOD_SHIFT, VK_F3);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_HOTKEY) {
            int idx = (int)msg.wParam - 1;
            if (idx >= 0 && idx <= 2) settings.toggle_layer_eye(idx);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    cfg.save();
    overlay.destroy();
    CoUninitialize();
    Gdiplus::GdiplusShutdown(g_gdiplusToken);
    return 0;
}
