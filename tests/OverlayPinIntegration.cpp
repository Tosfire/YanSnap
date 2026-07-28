#include <windows.h>

#include <iostream>
#include <memory>

#include "capture/DesktopCapture.h"
#include "overlay/OverlayWindow.h"

namespace {

std::unique_ptr<snaplite::OverlayWindow> g_overlay;
bool g_pinCallbackCalled = false;

LPARAM PackPoint(int x, int y) {
    return MAKELPARAM(static_cast<WORD>(x), static_cast<WORD>(y));
}

void RunPinAction(HWND owner) {
    HWND overlay = FindWindowW(L"YanSnap.OverlayWindow.v1", nullptr);
    if (!overlay) {
        DestroyWindow(owner);
        return;
    }
    SendMessageW(overlay, WM_LBUTTONDOWN, MK_LBUTTON, PackPoint(100, 100));
    SendMessageW(overlay, WM_MOUSEMOVE, MK_LBUTTON, PackPoint(420, 300));
    SendMessageW(overlay, WM_LBUTTONUP, 0, PackPoint(420, 300));
    RedrawWindow(overlay, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);

    // The pin button is index 7 in the 38 px toolbar.
    SendMessageW(overlay, WM_LBUTTONDOWN, MK_LBUTTON, PackPoint(385, 327));
    SendMessageW(overlay, WM_LBUTTONUP, 0, PackPoint(385, 327));
}

LRESULT CALLBACK OwnerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        SetTimer(window, 1, 150, nullptr);
        return 0;
    case WM_TIMER:
        KillTimer(window, 1);
        RunPinAction(window);
        return 0;
    case snaplite::OverlayWindow::kSessionEndedMessage:
        g_overlay.reset();
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(g_pinCallbackCalled ? 0 : 1);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

}  // namespace

int wmain() {
    SetProcessDPIAware();
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW ownerClass{};
    ownerClass.hInstance = instance;
    ownerClass.lpfnWndProc = OwnerProc;
    ownerClass.lpszClassName = L"YanSnap.PinTestOwner";
    RegisterClassW(&ownerClass);
    HWND owner = CreateWindowW(ownerClass.lpszClassName, L"", WS_OVERLAPPED,
                               0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!owner) {
        return 2;
    }

    snaplite::DesktopImage desktop = snaplite::DesktopCapture::Capture(false);
    snaplite::Settings settings;
    settings.showNotifications = false;
    g_overlay = std::make_unique<snaplite::OverlayWindow>(
        instance, owner, nullptr, settings,
        [](snaplite::ImageData image, POINT position) {
            g_pinCallbackCalled = image.width == 320 && image.height == 200 &&
                                  position.x == 100 && position.y == 100;
            std::wcout << L"pin-callback=" << g_pinCallbackCalled
                       << L" image=" << image.width << L"x" << image.height
                       << L" position=" << position.x << L"," << position.y << L"\n";
        });
    if (!g_overlay->Start(std::move(desktop))) {
        return 3;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

