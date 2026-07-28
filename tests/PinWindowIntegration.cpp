#include <windows.h>

#include <cstdint>
#include <iostream>
#include <memory>

#include "export/ClipboardExporter.h"
#include "pin/PinWindow.h"

namespace {

std::unique_ptr<snaplite::PinWindow> g_pin;
bool g_clipboardRoundTrip = false;
bool g_zoomed = false;
bool g_visibility = false;

snaplite::ImageData MakeTestImage() {
    snaplite::ImageData image;
    image.width = 240;
    image.height = 160;
    image.stride = image.width * 4;
    image.pixels.resize(static_cast<std::size_t>(image.stride) * image.height);
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            auto* pixel = image.pixels.data() +
                          static_cast<std::size_t>(y) * image.stride + x * 4;
            pixel[0] = static_cast<std::uint8_t>(40 + x * 120 / image.width);
            pixel[1] = static_cast<std::uint8_t>(80 + y * 130 / image.height);
            pixel[2] = static_cast<std::uint8_t>(220 - x * 100 / image.width);
            pixel[3] = 255;
        }
    }
    return image;
}

LRESULT CALLBACK OwnerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        SetTimer(window, 1, 180, nullptr);
        return 0;
    case WM_TIMER: {
        KillTimer(window, 1);
        HWND pinWindow = g_pin ? g_pin->Handle() : nullptr;
        if (!pinWindow) {
            DestroyWindow(window);
            return 0;
        }
        RECT before{};
        GetWindowRect(pinWindow, &before);
        const int centerX = (before.left + before.right) / 2;
        const int centerY = (before.top + before.bottom) / 2;
        SendMessageW(pinWindow, WM_MOUSEWHEEL,
                     MAKEWPARAM(0, WHEEL_DELTA), MAKELPARAM(centerX, centerY));
        RECT after{};
        GetWindowRect(pinWindow, &after);
        g_zoomed = after.right - after.left > before.right - before.left &&
                   after.bottom - after.top > before.bottom - before.top;

        g_pin->SetVisible(false);
        const bool hidden = !g_pin->IsVisible();
        g_pin->SetVisible(true);
        g_visibility = hidden && g_pin->IsVisible();
        SendMessageW(pinWindow, WM_KEYDOWN, VK_ESCAPE, 0);
        return 0;
    }
    case snaplite::PinWindow::kClosedMessage:
        g_pin.reset();
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        std::wcout << L"clipboard-roundtrip=" << g_clipboardRoundTrip
                   << L" zoom=" << g_zoomed
                   << L" visibility=" << g_visibility << L"\n";
        PostQuitMessage(g_clipboardRoundTrip && g_zoomed && g_visibility ? 0 : 1);
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
    ownerClass.lpszClassName = L"YanSnap.PinWindowTestOwner";
    RegisterClassW(&ownerClass);
    HWND owner = CreateWindowW(ownerClass.lpszClassName, L"", WS_OVERLAPPED,
                               0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!owner) {
        return 2;
    }

    snaplite::ImageData image = MakeTestImage();
    if (snaplite::ClipboardExporter::Copy(owner, image)) {
        auto imported = snaplite::ClipboardExporter::Read(owner);
        g_clipboardRoundTrip = imported && imported->width == image.width &&
                               imported->height == image.height &&
                               imported->pixels == image.pixels;
    }
    g_pin = std::make_unique<snaplite::PinWindow>(
        instance, owner, std::move(image), POINT{260, 180});
    if (!g_pin->Show()) {
        return 3;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
