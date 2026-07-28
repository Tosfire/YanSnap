#include "pin/PinWindow.h"

#include <commdlg.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <utility>
#include <vector>

#include "common/Win32.h"
#include "export/ClipboardExporter.h"
#include "export/PngEncoder.h"

namespace snaplite {

namespace {

constexpr wchar_t kPinWindowClassName[] = L"YanSnap.PinWindow.v1";
constexpr UINT kMenuCopy = 3101;
constexpr UINT kMenuSave = 3102;
constexpr UINT kMenuActualSize = 3103;
constexpr UINT kMenuOpacity100 = 3104;
constexpr UINT kMenuOpacity75 = 3105;
constexpr UINT kMenuOpacity50 = 3106;
constexpr UINT kMenuHide = 3107;
constexpr UINT kMenuClose = 3108;

BITMAPINFO MakeBitmapInfo(int width, int height) {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    return info;
}

}  // namespace

PinWindow::PinWindow(HINSTANCE instance, HWND owner, ImageData image, POINT screenPosition)
    : instance_(instance), owner_(owner), image_(std::move(image)),
      initialPosition_(screenPosition) {}

PinWindow::~PinWindow() {
    Close();
    DestroyImageSurface();
}

bool PinWindow::Show() {
    if (window_) {
        SetVisible(true);
        return true;
    }
    if (!image_.Valid() || !RegisterWindowClass() || !CreateImageSurface()) {
        return false;
    }

    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    GetMonitorInfoW(MonitorFromPoint(initialPosition_, MONITOR_DEFAULTTONEAREST), &monitorInfo);
    const int workWidth = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
    const int workHeight = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
    scale_ = std::min({1.0, workWidth * 0.85 / image_.width,
                       workHeight * 0.85 / image_.height});
    const int width = std::max(48, static_cast<int>(std::lround(image_.width * scale_)));
    const int height = std::max(32, static_cast<int>(std::lround(image_.height * scale_)));
    int left = initialPosition_.x;
    int top = initialPosition_.y;
    ClampToWorkingArea(left, top, width, height);

    window_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                              kPinWindowClassName, L"YanSnap 贴图", WS_POPUP,
                              left, top, width, height, owner_, nullptr, instance_, this);
    if (!window_) {
        DestroyImageSurface();
        return false;
    }
    SetLayeredWindowAttributes(window_, 0, opacity_, LWA_ALPHA);
    ShowWindow(window_, SW_SHOWNOACTIVATE);
    UpdateWindow(window_);
    return true;
}

void PinWindow::Close() {
    if (window_) {
        DestroyWindow(window_);
    }
}

void PinWindow::SetVisible(bool visible) {
    if (window_) {
        ShowWindow(window_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (visible) {
            SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
}

bool PinWindow::IsVisible() const {
    return window_ && IsWindowVisible(window_);
}

bool PinWindow::RegisterWindowClass() {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_DBLCLKS;
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = kPinWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_SIZEALL);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    return RegisterClassExW(&windowClass) != 0 ||
           GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool PinWindow::CreateImageSurface() {
    imageDc_ = CreateCompatibleDC(nullptr);
    if (!imageDc_) {
        return false;
    }
    BITMAPINFO info = MakeBitmapInfo(image_.width, image_.height);
    void* bits = nullptr;
    imageBitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!imageBitmap_ || !bits) {
        DestroyImageSurface();
        return false;
    }
    std::memcpy(bits, image_.pixels.data(), image_.pixels.size());
    oldImageBitmap_ = SelectObject(imageDc_, imageBitmap_);
    return true;
}

void PinWindow::DestroyImageSurface() {
    if (imageDc_ && oldImageBitmap_) {
        SelectObject(imageDc_, oldImageBitmap_);
    }
    if (imageBitmap_) {
        DeleteObject(imageBitmap_);
    }
    if (imageDc_) {
        DeleteDC(imageDc_);
    }
    imageDc_ = nullptr;
    imageBitmap_ = nullptr;
    oldImageBitmap_ = nullptr;
}

LRESULT CALLBACK PinWindow::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* pin = reinterpret_cast<PinWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pin = static_cast<PinWindow*>(create->lpCreateParams);
        pin->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pin));
    }
    return pin ? pin->HandleMessage(message, wParam, lParam)
               : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT PinWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Paint();
        return 0;
    case WM_LBUTTONDOWN:
        SetForegroundWindow(window_);
        BeginDrag();
        return 0;
    case WM_MOUSEMOVE:
        if (dragging_) {
            ContinueDrag();
        } else {
            if (!hovered_) {
                hovered_ = true;
                InvalidateRect(window_, nullptr, FALSE);
            }
            if (!trackingMouseLeave_) {
                TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window_, 0};
                trackingMouseLeave_ = TrackMouseEvent(&tracking) != FALSE;
            }
        }
        return 0;
    case WM_MOUSELEAVE:
        trackingMouseLeave_ = false;
        hovered_ = false;
        InvalidateRect(window_, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP:
        EndDrag();
        return 0;
    case WM_LBUTTONDBLCLK:
        SetVisible(false);
        return 0;
    case WM_RBUTTONUP: {
        POINT point{};
        GetCursorPos(&point);
        ShowContextMenu(point);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if ((GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0) {
            AdjustOpacity(delta > 0 ? 15 : -15);
        } else {
            ZoomAt(point, delta > 0 ? 1.1 : 1.0 / 1.1);
        }
        return 0;
    }
    case WM_KEYDOWN: {
        const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (wParam == VK_ESCAPE) {
            Close();
        } else if (control && wParam == 'C') {
            ClipboardExporter::Copy(window_, image_);
        } else if (control && wParam == 'S') {
            SaveToFile();
        } else if (wParam == VK_ADD || wParam == VK_OEM_PLUS) {
            POINT point{};
            GetCursorPos(&point);
            ZoomAt(point, 1.1);
        } else if (wParam == VK_SUBTRACT || wParam == VK_OEM_MINUS) {
            POINT point{};
            GetCursorPos(&point);
            ZoomAt(point, 1.0 / 1.1);
        }
        return 0;
    }
    case WM_DESTROY:
        if (GetCapture() == window_) {
            ReleaseCapture();
        }
        window_ = nullptr;
        DestroyImageSurface();
        PostMessageW(owner_, kClosedMessage, reinterpret_cast<WPARAM>(this), 0);
        return 0;
    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
}

void PinWindow::Paint() {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window_, &paint);
    RECT client{};
    GetClientRect(window_, &client);
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, nullptr);
    StretchBlt(dc, 0, 0, client.right, client.bottom, imageDc_,
               0, 0, image_.width, image_.height, SRCCOPY);
    if (hovered_) {
        UniquePen border(CreatePen(PS_SOLID, 1, RGB(32, 137, 255)));
        SelectObjectGuard selectedPen(dc, border.get());
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, 0, 0, client.right, client.bottom);
        SelectObject(dc, oldBrush);
    }
    EndPaint(window_, &paint);
}

void PinWindow::BeginDrag() {
    dragging_ = true;
    GetCursorPos(&dragCursorStart_);
    RECT bounds{};
    GetWindowRect(window_, &bounds);
    dragWindowStart_ = POINT{bounds.left, bounds.top};
    SetCapture(window_);
}

void PinWindow::ContinueDrag() {
    POINT cursor{};
    GetCursorPos(&cursor);
    const int left = dragWindowStart_.x + cursor.x - dragCursorStart_.x;
    const int top = dragWindowStart_.y + cursor.y - dragCursorStart_.y;
    SetWindowPos(window_, HWND_TOPMOST, left, top, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE);
}

void PinWindow::EndDrag() {
    dragging_ = false;
    if (GetCapture() == window_) {
        ReleaseCapture();
    }
}

void PinWindow::ZoomAt(POINT screenPoint, double factor) {
    RECT bounds{};
    GetWindowRect(window_, &bounds);
    const int oldWidth = bounds.right - bounds.left;
    const int oldHeight = bounds.bottom - bounds.top;
    const double anchorX = oldWidth > 0 ? (screenPoint.x - bounds.left) /
                                             static_cast<double>(oldWidth) : 0.5;
    const double anchorY = oldHeight > 0 ? (screenPoint.y - bounds.top) /
                                              static_cast<double>(oldHeight) : 0.5;
    scale_ = std::clamp(scale_ * factor, 0.12, 5.0);
    const int width = std::max(48, static_cast<int>(std::lround(image_.width * scale_)));
    const int height = std::max(32, static_cast<int>(std::lround(image_.height * scale_)));
    int left = static_cast<int>(std::lround(screenPoint.x - anchorX * width));
    int top = static_cast<int>(std::lround(screenPoint.y - anchorY * height));
    ClampToWorkingArea(left, top, width, height);
    SetWindowPos(window_, HWND_TOPMOST, left, top, width, height,
                 SWP_NOACTIVATE);
    InvalidateRect(window_, nullptr, FALSE);
}

void PinWindow::ResetSize() {
    scale_ = 1.0;
    RECT bounds{};
    GetWindowRect(window_, &bounds);
    int left = bounds.left;
    int top = bounds.top;
    ClampToWorkingArea(left, top, image_.width, image_.height);
    SetWindowPos(window_, HWND_TOPMOST, left, top, image_.width, image_.height,
                 SWP_NOACTIVATE);
    InvalidateRect(window_, nullptr, FALSE);
}

void PinWindow::AdjustOpacity(int delta) {
    opacity_ = static_cast<BYTE>(
        std::clamp(static_cast<int>(opacity_) + delta, 40, 255));
    SetLayeredWindowAttributes(window_, 0, opacity_, LWA_ALPHA);
}

void PinWindow::ShowContextMenu(POINT screenPoint) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }
    AppendMenuW(menu, MF_STRING, kMenuCopy, L"复制图像\tCtrl+C");
    AppendMenuW(menu, MF_STRING, kMenuSave, L"另存为…\tCtrl+S");
    AppendMenuW(menu, MF_STRING, kMenuActualSize, L"恢复 100% 大小");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    HMENU opacityMenu = CreatePopupMenu();
    AppendMenuW(opacityMenu, MF_STRING, kMenuOpacity100, L"100%");
    AppendMenuW(opacityMenu, MF_STRING, kMenuOpacity75, L"75%");
    AppendMenuW(opacityMenu, MF_STRING, kMenuOpacity50, L"50%");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(opacityMenu), L"不透明度");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuHide, L"隐藏（双击）");
    AppendMenuW(menu, MF_STRING, kMenuClose, L"关闭\tEsc");
    SetForegroundWindow(window_);
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                        screenPoint.x, screenPoint.y, 0, window_, nullptr);
    DestroyMenu(menu);
    switch (command) {
    case kMenuCopy:
        ClipboardExporter::Copy(window_, image_);
        break;
    case kMenuSave:
        SaveToFile();
        break;
    case kMenuActualSize:
        ResetSize();
        break;
    case kMenuOpacity100:
        opacity_ = 255;
        SetLayeredWindowAttributes(window_, 0, opacity_, LWA_ALPHA);
        break;
    case kMenuOpacity75:
        opacity_ = 191;
        SetLayeredWindowAttributes(window_, 0, opacity_, LWA_ALPHA);
        break;
    case kMenuOpacity50:
        opacity_ = 128;
        SetLayeredWindowAttributes(window_, 0, opacity_, LWA_ALPHA);
        break;
    case kMenuHide:
        SetVisible(false);
        break;
    case kMenuClose:
        Close();
        break;
    default:
        break;
    }
}

void PinWindow::SaveToFile() {
    const std::wstring directory = PngEncoder::DefaultScreenshotsDirectory();
    std::filesystem::path initial =
        std::filesystem::path(directory) / PngEncoder::SuggestedFileName(directory);
    std::vector<wchar_t> path(32768, L'\0');
    lstrcpynW(path.data(), initial.c_str(), static_cast<int>(path.size()));
    constexpr wchar_t filter[] = L"PNG 图片 (*.png)\0*.png\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = L"png";
    dialog.lpstrInitialDir = directory.c_str();
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&dialog)) {
        return;
    }
    std::wstring error;
    if (!PngEncoder::Save(path.data(), image_, &error)) {
        MessageBoxW(window_, error.c_str(), L"YanSnap", MB_OK | MB_ICONERROR);
    }
}

void PinWindow::ClampToWorkingArea(int& left, int& top, int width, int height) const {
    POINT center{left + width / 2, top + height / 2};
    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    GetMonitorInfoW(MonitorFromPoint(center, MONITOR_DEFAULTTONEAREST), &monitorInfo);
    const RECT work = monitorInfo.rcWork;
    const int visibleWidth = std::min(width, 48);
    const int visibleHeight = std::min(height, 32);
    left = std::clamp(left, static_cast<int>(work.left) - width + visibleWidth,
                      static_cast<int>(work.right) - visibleWidth);
    top = std::clamp(top, static_cast<int>(work.top) - height + visibleHeight,
                     static_cast<int>(work.bottom) - visibleHeight);
}

}  // namespace snaplite
