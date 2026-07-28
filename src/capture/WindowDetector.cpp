#include "capture/WindowDetector.h"

#include <dwmapi.h>

#include <array>

namespace snaplite {

namespace {

bool IsUsableWindow(HWND window, DWORD excludedProcessId) {
    if (!IsWindowVisible(window) || IsIconic(window)) {
        return false;
    }
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId == excludedProcessId) {
        return false;
    }
    const LONG_PTR extendedStyle = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if ((extendedStyle & WS_EX_TRANSPARENT) != 0 || (extendedStyle & WS_EX_TOOLWINDOW) != 0) {
        return false;
    }
    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) {
        return false;
    }
    std::array<wchar_t, 64> className{};
    GetClassNameW(window, className.data(), static_cast<int>(className.size()));
    if (lstrcmpW(className.data(), L"Progman") == 0 ||
        lstrcmpW(className.data(), L"WorkerW") == 0 ||
        lstrcmpW(className.data(), L"Shell_TrayWnd") == 0) {
        return false;
    }
    return true;
}

std::optional<RectI> WindowBounds(HWND window, RectI desktopBounds) {
    RECT bounds{};
    if (FAILED(DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds))) &&
        !GetWindowRect(window, &bounds)) {
        return std::nullopt;
    }
    RectI clipped = Intersect(RectI::FromRect(bounds), desktopBounds);
    if (clipped.Width() < 2 || clipped.Height() < 2) {
        return std::nullopt;
    }
    return clipped;
}

}  // namespace

std::optional<RectI> WindowDetector::Detect(POINT screenPoint, DWORD excludedProcessId,
                                            RectI desktopBounds) {
    for (HWND window = GetTopWindow(nullptr); window; window = GetWindow(window, GW_HWNDNEXT)) {
        if (!IsUsableWindow(window, excludedProcessId)) {
            continue;
        }
        auto bounds = WindowBounds(window, desktopBounds);
        if (bounds && bounds->Contains(screenPoint)) {
            return bounds;
        }
    }
    return std::nullopt;
}

}  // namespace snaplite

