#include <windows.h>

#include <iostream>
#include <memory>
#include <filesystem>

#include "capture/DesktopCapture.h"
#include "overlay/OverlayWindow.h"

namespace {

std::unique_ptr<snaplite::OverlayWindow> g_overlay;
bool g_passed = false;
std::size_t g_existingPngs = 0;

std::size_t CountSavedPngs() {
    std::error_code error;
    std::size_t count = 0;
    const std::filesystem::path directory = L"build\\integration-saves";
    if (!std::filesystem::exists(directory, error)) {
        return 0;
    }
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (entry.is_regular_file() && entry.path().extension() == L".png") {
            ++count;
        }
    }
    return count;
}

LPARAM PackPoint(int x, int y) {
    return MAKELPARAM(static_cast<WORD>(x), static_cast<WORD>(y));
}

void RunSelection(HWND owner) {
    HWND overlayWindow = FindWindowW(L"YanSnap.OverlayWindow.v1", nullptr);
    if (!overlayWindow) {
        std::wcerr << L"overlay window not found\n";
        DestroyWindow(owner);
        return;
    }
    SendMessageW(overlayWindow, WM_LBUTTONDOWN, MK_LBUTTON, PackPoint(100, 100));
    SendMessageW(overlayWindow, WM_MOUSEMOVE, MK_LBUTTON, PackPoint(420, 300));
    SendMessageW(overlayWindow, WM_LBUTTONUP, 0, PackPoint(420, 300));
    RedrawWindow(overlayWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);

    // First toolbar button selects the rectangle tool.
    SendMessageW(overlayWindow, WM_LBUTTONDOWN, MK_LBUTTON, PackPoint(118, 326));
    SendMessageW(overlayWindow, WM_LBUTTONUP, 0, PackPoint(118, 326));
    SendMessageW(overlayWindow, WM_LBUTTONDOWN, MK_LBUTTON, PackPoint(130, 130));
    SendMessageW(overlayWindow, WM_MOUSEMOVE, MK_LBUTTON, PackPoint(300, 230));
    SendMessageW(overlayWindow, WM_LBUTTONUP, 0, PackPoint(300, 230));
    SendMessageW(overlayWindow, WM_KEYDOWN, VK_RETURN, 0);
}

void CheckClipboard() {
    if (!OpenClipboard(nullptr)) {
        std::wcerr << L"clipboard could not be opened\n";
        return;
    }
    HGLOBAL memory = static_cast<HGLOBAL>(GetClipboardData(CF_DIBV5));
    if (memory) {
        const auto* header = static_cast<const BITMAPV5HEADER*>(GlobalLock(memory));
        if (header) {
            std::wcout << L"clipboard=" << header->bV5Width << L"x" << header->bV5Height << L"\n";
            const auto* pixels = reinterpret_cast<const std::uint8_t*>(header) + header->bV5Size;
            const std::size_t pixel = (30U * 320U + 30U) * 4U;
            const bool redBorder = pixels[pixel + 2] > 200 && pixels[pixel + 1] < 110 &&
                                   pixels[pixel] < 110;
            std::wcout << L"rectangle-border=" << redBorder << L"\n";
            g_passed = header->bV5Width == 320 && header->bV5Height == -200 && redBorder;
            GlobalUnlock(memory);
        }
    }
    CloseClipboard();
}

LRESULT CALLBACK OwnerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        SetTimer(window, 1, 150, nullptr);
        return 0;
    case WM_TIMER:
        KillTimer(window, 1);
        RunSelection(window);
        return 0;
    case snaplite::OverlayWindow::kSessionEndedMessage: {
        CheckClipboard();
        const bool asyncSaveOk = CountSavedPngs() > g_existingPngs;
        std::wcout << L"async-save=" << asyncSaveOk << L"\n";
        g_passed = g_passed && asyncSaveOk;
        g_overlay.reset();
        DestroyWindow(window);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(g_passed ? 0 : 1);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

}  // namespace

int wmain() {
    SetProcessDPIAware();
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = OwnerProc;
    windowClass.lpszClassName = L"YanSnap.TestOwner";
    RegisterClassW(&windowClass);
    HWND owner = CreateWindowW(windowClass.lpszClassName, L"", WS_OVERLAPPED,
                               0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!owner) {
        return 2;
    }

    snaplite::DesktopImage desktop = snaplite::DesktopCapture::Capture(false);
    snaplite::Settings settings;
    settings.defaultAction = snaplite::DefaultAction::CopyAndAutoSave;
    settings.saveDirectory = L"build\\integration-saves";
    settings.showNotifications = false;
    g_existingPngs = CountSavedPngs();
    g_overlay = std::make_unique<snaplite::OverlayWindow>(
        instance, owner, nullptr, settings);
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
