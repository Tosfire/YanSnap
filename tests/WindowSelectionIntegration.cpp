#include <windows.h>
#include <dwmapi.h>

#include <iostream>
#include <memory>
#include <filesystem>
#include <vector>

#include "capture/DesktopCapture.h"
#include "overlay/OverlayWindow.h"

namespace {

std::unique_ptr<snaplite::OverlayWindow> g_overlay;
RECT g_targetBounds{};
PROCESS_INFORMATION g_targetProcess{};
bool g_passed = false;

LPARAM PackPoint(int x, int y) {
    return MAKELPARAM(static_cast<WORD>(x), static_cast<WORD>(y));
}

void RunWindowSelection(HWND owner) {
    HWND overlay = FindWindowW(L"YanSnap.OverlayWindow.v1", nullptr);
    if (!overlay) {
        DestroyWindow(owner);
        return;
    }
    const int desktopX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int desktopY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int screenX = (g_targetBounds.left + g_targetBounds.right) / 2;
    const int screenY = (g_targetBounds.top + g_targetBounds.bottom) / 2;
    const int clientX = screenX - desktopX;
    const int clientY = screenY - desktopY;
    SendMessageW(overlay, WM_LBUTTONDOWN, MK_LBUTTON, PackPoint(clientX, clientY));
    SendMessageW(overlay, WM_LBUTTONUP, 0, PackPoint(clientX, clientY));
    SendMessageW(overlay, WM_KEYDOWN, VK_RETURN, 0);
}

void CheckClipboard() {
    if (!OpenClipboard(nullptr)) {
        return;
    }
    HGLOBAL memory = static_cast<HGLOBAL>(GetClipboardData(CF_DIBV5));
    if (memory) {
        const auto* header = static_cast<const BITMAPV5HEADER*>(GlobalLock(memory));
        if (header) {
            const int expectedWidth = g_targetBounds.right - g_targetBounds.left;
            const int expectedHeight = g_targetBounds.bottom - g_targetBounds.top;
            g_passed = header->bV5Width == expectedWidth &&
                       header->bV5Height == -expectedHeight;
            std::wcout << L"window-selection=" << header->bV5Width << L"x"
                       << header->bV5Height << L" expected=" << expectedWidth
                       << L"x-" << expectedHeight << L"\n";
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
        RunWindowSelection(window);
        return 0;
    case snaplite::OverlayWindow::kSessionEndedMessage:
        CheckClipboard();
        g_overlay.reset();
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(g_passed ? 0 : 1);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

bool StartTargetWindow() {
    std::vector<wchar_t> modulePath(32768, L'\0');
    GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    const std::filesystem::path helper =
        std::filesystem::path(modulePath.data()).parent_path() / L"TestWindowHost.exe";
    std::wstring command = L"\"" + helper.wstring() + L"\"";
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{sizeof(startup)};
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &startup, &g_targetProcess)) {
        return false;
    }
    HWND target = nullptr;
    for (int attempt = 0; attempt < 50 && !target; ++attempt) {
        Sleep(50);
        target = FindWindowW(L"YanSnap.TestTarget", nullptr);
    }
    if (!target) {
        return false;
    }
    if (FAILED(DwmGetWindowAttribute(target, DWMWA_EXTENDED_FRAME_BOUNDS,
                                     &g_targetBounds, sizeof(g_targetBounds)))) {
        GetWindowRect(target, &g_targetBounds);
    }
    return true;
}

void StopTargetWindow() {
    HWND target = FindWindowW(L"YanSnap.TestTarget", nullptr);
    if (target) {
        PostMessageW(target, WM_CLOSE, 0, 0);
    }
    if (g_targetProcess.hProcess) {
        WaitForSingleObject(g_targetProcess.hProcess, 2000);
        CloseHandle(g_targetProcess.hProcess);
    }
    if (g_targetProcess.hThread) {
        CloseHandle(g_targetProcess.hThread);
    }
}

}  // namespace

int wmain() {
    SetProcessDPIAware();
    if (!StartTargetWindow()) {
        StopTargetWindow();
        return 2;
    }
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW ownerClass{};
    ownerClass.hInstance = instance;
    ownerClass.lpfnWndProc = OwnerProc;
    ownerClass.lpszClassName = L"YanSnap.WindowTestOwner";
    RegisterClassW(&ownerClass);
    HWND owner = CreateWindowW(ownerClass.lpszClassName, L"", WS_OVERLAPPED,
                               0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    snaplite::DesktopImage desktop = snaplite::DesktopCapture::Capture(false);
    snaplite::Settings settings;
    settings.showNotifications = false;
    g_overlay = std::make_unique<snaplite::OverlayWindow>(
        instance, owner, nullptr, settings);
    if (!g_overlay->Start(std::move(desktop))) {
        StopTargetWindow();
        return 3;
    }
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    StopTargetWindow();
    return static_cast<int>(message.wParam);
}
