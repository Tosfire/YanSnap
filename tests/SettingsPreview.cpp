#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <cstring>
#include <string_view>

#include "export/ImageData.h"
#include "export/PngEncoder.h"
#include "settings/Settings.h"
#include "settings/SettingsWindow.h"

namespace {

LRESULT CALLBACK OwnerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool RenderWindow(HWND window, const wchar_t* outputPath) {
    RECT bounds{};
    GetWindowRect(window, &bounds);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bitmap =
        CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!dc || !bitmap || !pixels) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        if (dc) {
            DeleteDC(dc);
        }
        return false;
    }

    HGDIOBJ previous = SelectObject(dc, bitmap);
    const BOOL printed = PrintWindow(window, dc, PW_RENDERFULLCONTENT);
    snaplite::ImageData image;
    image.width = width;
    image.height = height;
    image.stride = width * 4;
    image.pixels.resize(static_cast<std::size_t>(image.stride) * height);
    std::memcpy(image.pixels.data(), pixels, image.pixels.size());
    const bool saved = printed &&
                       snaplite::PngEncoder::Save(outputPath, image);
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return saved;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX controls{
        sizeof(controls), ICC_STANDARD_CLASSES | ICC_HOTKEY_CLASS};
    InitCommonControlsEx(&controls);

    WNDCLASSW ownerClass{};
    ownerClass.hInstance = instance;
    ownerClass.lpfnWndProc = OwnerProc;
    ownerClass.lpszClassName = L"YanSnap.SettingsPreviewOwner";
    RegisterClassW(&ownerClass);
    HWND owner = CreateWindowW(ownerClass.lpszClassName, L"", WS_OVERLAPPED,
                               0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!owner) {
        return 1;
    }

    snaplite::SettingsWindow settingsWindow(
        instance, owner, snaplite::Settings::Defaults(),
        [](const snaplite::Settings&) { return true; });
    if (!settingsWindow.Show()) {
        DestroyWindow(owner);
        return 1;
    }

    int argumentCount = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    const bool render = argumentCount == 3 &&
                        std::wstring_view(arguments[1]) == L"--render";
    if (render) {
        HWND settings = FindWindowW(L"YanSnap.SettingsWindow.v2", nullptr);
        MSG pending{};
        while (PeekMessageW(&pending, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&pending);
            DispatchMessageW(&pending);
        }
        if (settings) {
            RedrawWindow(settings, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        }
        const bool saved = settings && RenderWindow(settings, arguments[2]);
        if (settings) {
            SendMessageW(settings, WM_CLOSE, 0, 0);
        }
        LocalFree(arguments);
        DestroyWindow(owner);
        return saved ? 0 : 1;
    }
    LocalFree(arguments);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
        if (!settingsWindow.Active()) {
            break;
        }
    }
    DestroyWindow(owner);
    return 0;
}
