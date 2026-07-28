#include "overlay/OverlayWindow.h"

#include <commdlg.h>
#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>

#include "annotation/Annotation.h"
#include "app/TrayIcon.h"
#include "capture/WindowDetector.h"
#include "export/ClipboardExporter.h"
#include "export/ImageComposer.h"
#include "export/PngEncoder.h"

namespace snaplite {

namespace {

constexpr wchar_t kOverlayClassName[] = L"YanSnap.OverlayWindow.v1";
constexpr COLORREF kSelectionColor = RGB(32, 137, 255);
constexpr UINT_PTR kToolbarTooltipTimer = 1;

struct AsyncSaveResult {
    bool success{};
    bool copied{};
    std::wstring error;
};

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

int DpiScaled(HWND window, int logicalPixels) {
    const UINT dpi = GetDpiForWindow(window);
    return MulDiv(logicalPixels, static_cast<int>(dpi), 96);
}

}  // namespace

OverlayWindow::OverlayWindow(HINSTANCE instance, HWND owner, TrayIcon* tray, Settings settings,
                             PinCallback pinCallback)
    : instance_(instance), owner_(owner), tray_(tray), settings_(std::move(settings)),
      pinCallback_(std::move(pinCallback)) {}

OverlayWindow::~OverlayWindow() {
    if (saveThread_.joinable()) {
        saveThread_.join();
    }
    delete static_cast<AsyncSaveResult*>(pendingSaveResult_.exchange(nullptr));
    Cancel();
    DestroyPaintSurfaces();
}

bool OverlayWindow::Start(DesktopImage image) {
    lastErrorCode_ = ERROR_SUCCESS;
    if (window_ || !image.Valid()) {
        lastErrorCode_ = ERROR_INVALID_DATA;
        return false;
    }
    desktop_ = std::move(image);
    if (!RegisterWindowClass()) {
        lastErrorCode_ = GetLastError();
        desktop_ = {};
        return false;
    }
    if (!CreatePaintSurfaces()) {
        lastErrorCode_ = ERROR_NOT_ENOUGH_MEMORY;
        desktop_ = {};
        return false;
    }

    window_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kOverlayClassName, L"YanSnap",
                              WS_POPUP, desktop_.originX, desktop_.originY,
                              desktop_.width, desktop_.height,
                              owner_, nullptr, instance_, this);
    if (!window_) {
        lastErrorCode_ = GetLastError();
        DestroyPaintSurfaces();
        desktop_ = {};
        return false;
    }
    ShowWindow(window_, SW_SHOW);
    UpdateWindow(window_);
    SetForegroundWindow(window_);
    SetFocus(window_);
    return true;
}

void OverlayWindow::Cancel() {
    if (window_) {
        DestroyWindow(window_);
    }
}

bool OverlayWindow::RegisterWindowClass() {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_DBLCLKS;
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = kOverlayClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    return RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool OverlayWindow::CreatePaintSurfaces() {
    originalDc_ = CreateCompatibleDC(nullptr);
    dimmedDc_ = CreateCompatibleDC(nullptr);
    backBufferDc_ = CreateCompatibleDC(nullptr);
    if (!originalDc_ || !dimmedDc_ || !backBufferDc_) {
        DestroyPaintSurfaces();
        return false;
    }

    BITMAPINFO info = MakeBitmapInfo(desktop_.width, desktop_.height);
    void* originalBits = nullptr;
    void* dimmedBits = nullptr;
    void* backBufferBits = nullptr;
    originalBitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &originalBits, nullptr, 0);
    dimmedBitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &dimmedBits, nullptr, 0);
    backBufferBitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS,
                                         &backBufferBits, nullptr, 0);
    if (!originalBitmap_ || !dimmedBitmap_ || !backBufferBitmap_ ||
        !originalBits || !dimmedBits || !backBufferBits) {
        DestroyPaintSurfaces();
        return false;
    }

    const std::size_t bytes = desktop_.pixels.size();
    std::memcpy(originalBits, desktop_.pixels.data(), bytes);
    auto* dimmed = static_cast<std::uint8_t*>(dimmedBits);
    for (std::size_t index = 0; index < bytes; index += 4) {
        dimmed[index] = static_cast<std::uint8_t>(desktop_.pixels[index] * 55 / 100);
        dimmed[index + 1] = static_cast<std::uint8_t>(desktop_.pixels[index + 1] * 55 / 100);
        dimmed[index + 2] = static_cast<std::uint8_t>(desktop_.pixels[index + 2] * 55 / 100);
        dimmed[index + 3] = 255;
    }
    oldOriginalBitmap_ = SelectObject(originalDc_, originalBitmap_);
    oldDimmedBitmap_ = SelectObject(dimmedDc_, dimmedBitmap_);
    oldBackBufferBitmap_ = SelectObject(backBufferDc_, backBufferBitmap_);
    return true;
}

void OverlayWindow::DestroyPaintSurfaces() {
    if (originalDc_ && oldOriginalBitmap_) {
        SelectObject(originalDc_, oldOriginalBitmap_);
    }
    if (dimmedDc_ && oldDimmedBitmap_) {
        SelectObject(dimmedDc_, oldDimmedBitmap_);
    }
    if (backBufferDc_ && oldBackBufferBitmap_) {
        SelectObject(backBufferDc_, oldBackBufferBitmap_);
    }
    if (originalBitmap_) {
        DeleteObject(originalBitmap_);
    }
    if (dimmedBitmap_) {
        DeleteObject(dimmedBitmap_);
    }
    if (backBufferBitmap_) {
        DeleteObject(backBufferBitmap_);
    }
    if (originalDc_) {
        DeleteDC(originalDc_);
    }
    if (dimmedDc_) {
        DeleteDC(dimmedDc_);
    }
    if (backBufferDc_) {
        DeleteDC(backBufferDc_);
    }
    originalDc_ = nullptr;
    dimmedDc_ = nullptr;
    backBufferDc_ = nullptr;
    originalBitmap_ = nullptr;
    dimmedBitmap_ = nullptr;
    backBufferBitmap_ = nullptr;
    oldOriginalBitmap_ = nullptr;
    oldDimmedBitmap_ = nullptr;
    oldBackBufferBitmap_ = nullptr;
}

LRESULT CALLBACK OverlayWindow::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* overlay = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        overlay = static_cast<OverlayWindow*>(create->lpCreateParams);
        overlay->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
    }
    return overlay ? overlay->HandleMessage(message, wParam, lParam)
                   : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK OverlayWindow::TextEditSubclassProc(HWND window, UINT message,
                                                      WPARAM wParam, LPARAM lParam,
                                                      UINT_PTR subclassId,
                                                      DWORD_PTR referenceData) {
    auto* overlay = reinterpret_cast<OverlayWindow*>(referenceData);
    if (message == WM_KEYDOWN && wParam == VK_ESCAPE) {
        overlay->CancelTextInput();
        return 0;
    }
    if (message == WM_KEYDOWN && wParam == VK_RETURN &&
        (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
        overlay->CommitTextInput();
        return 0;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, TextEditSubclassProc, subclassId);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

LRESULT OverlayWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Paint();
        return 0;
    case WM_TIMER:
        if (wParam == kToolbarTooltipTimer) {
            KillTimer(window_, kToolbarTooltipTimer);
            showToolbarTooltip_ = hoveredToolbarAction_ != ToolbarAction::None;
            InvalidateRect(window_, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSELEAVE:
        trackingMouseLeave_ = false;
        UpdateToolbarHover(ToolbarAction::None);
        return 0;
    case kAsyncSaveCompleteMessage:
        HandleAsyncSaveComplete(reinterpret_cast<void*>(lParam));
        return 0;
    case WM_LBUTTONDOWN:
        if (saving_) {
            return 0;
        }
        OnLeftButtonDown(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return 0;
    case WM_MOUSEMOVE:
        if (saving_) {
            return 0;
        }
        OnMouseMove(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, wParam);
        return 0;
    case WM_LBUTTONUP:
        if (saving_) {
            return 0;
        }
        OnLeftButtonUp(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return 0;
    case WM_LBUTTONDBLCLK:
        if (selection_) {
            CopyAndFinish();
        }
        return 0;
    case WM_RBUTTONUP:
        if (saving_) {
            return 0;
        }
        if (activeAnnotation_) {
            activeAnnotation_.reset();
            dragMode_ = DragMode::None;
            InvalidateRect(window_, nullptr, FALSE);
        } else if (textInput_) {
            CancelTextInput();
        } else {
            Cancel();
        }
        return 0;
    case WM_COMMAND:
        if (reinterpret_cast<HWND>(lParam) == textInput_ && HIWORD(wParam) == EN_KILLFOCUS) {
            CommitTextInput();
        }
        return 0;
    case WM_KEYDOWN: {
        if (saving_) {
            return 0;
        }
        const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 10 : 1;
        if (wParam == VK_ESCAPE) {
            if (activeAnnotation_) {
                activeAnnotation_.reset();
                dragMode_ = DragMode::None;
                InvalidateRect(window_, nullptr, FALSE);
            } else {
                Cancel();
            }
        } else if (wParam == VK_RETURN || (control && wParam == 'C')) {
            CopyAndFinish();
        } else if (control && wParam == 'S') {
            SaveAndFinish();
        } else if (control && wParam == 'T') {
            PinAndFinish();
        } else if (control && wParam == 'Z') {
            annotations_.Undo();
            selectedAnnotation_.reset();
            InvalidateRect(window_, nullptr, FALSE);
        } else if (control && wParam == 'Y') {
            annotations_.Redo();
            selectedAnnotation_.reset();
            InvalidateRect(window_, nullptr, FALSE);
        } else if (wParam == VK_DELETE && selectedAnnotation_) {
            annotations_.Delete(*selectedAnnotation_);
            selectedAnnotation_.reset();
            InvalidateRect(window_, nullptr, FALSE);
        } else if (wParam == VK_LEFT) {
            MoveSelection(-step, 0);
        } else if (wParam == VK_RIGHT) {
            MoveSelection(step, 0);
        } else if (wParam == VK_UP) {
            MoveSelection(0, -step);
        } else if (wParam == VK_DOWN) {
            MoveSelection(0, step);
        }
        return 0;
    }
    case WM_DISPLAYCHANGE:
        MessageBoxW(window_, L"显示配置已改变，本次截图已取消。", L"YanSnap",
                    MB_OK | MB_ICONINFORMATION);
        Cancel();
        return 0;
    case WM_DESTROY:
        if (GetCapture() == window_) {
            ReleaseCapture();
        }
        window_ = nullptr;
        if (textInputFont_) {
            DeleteObject(textInputFont_);
            textInputFont_ = nullptr;
        }
        desktop_ = {};
        DestroyPaintSurfaces();
        PostMessageW(owner_, kSessionEndedMessage, 0, 0);
        return 0;
    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
}

void OverlayWindow::Paint() {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window_, &paint);
    HDC renderDc = backBufferDc_;
    BitBlt(renderDc, 0, 0, desktop_.width, desktop_.height, dimmedDc_, 0, 0, SRCCOPY);

    if (selection_) {
        RectI client = ScreenToClientRect(*selection_);
        BitBlt(renderDc, client.left, client.top, client.Width(), client.Height(),
               originalDc_, client.left, client.top, SRCCOPY);
        DrawAnnotations(renderDc);
        DrawSelection(renderDc, *selection_, true);
        DrawDimensionLabel(renderDc, *selection_);
        toolbar_.Update(ScreenToClientRect(*selection_),
                        RectI{0, 0, desktop_.width, desktop_.height}, CurrentDpi());
        toolbar_.Draw(renderDc, currentTool_, annotations_.CanUndo(), annotations_.CanRedo(),
                      hoveredToolbarAction_, showToolbarTooltip_,
                      RectI{0, 0, desktop_.width, desktop_.height});
        if (dragMode_ == DragMode::Creating || dragMode_ == DragMode::Resizing ||
            dragMode_ == DragMode::Annotating) {
            DrawMagnifier(renderDc);
        }
    } else if (hoverWindow_) {
        RectI client = ScreenToClientRect(*hoverWindow_);
        BitBlt(renderDc, client.left, client.top, client.Width(), client.Height(),
               originalDc_, client.left, client.top, SRCCOPY);
        DrawSelection(renderDc, *hoverWindow_, false);
    }
    BitBlt(dc, paint.rcPaint.left, paint.rcPaint.top,
           paint.rcPaint.right - paint.rcPaint.left,
           paint.rcPaint.bottom - paint.rcPaint.top,
           renderDc, paint.rcPaint.left, paint.rcPaint.top, SRCCOPY);
    EndPaint(window_, &paint);
}

void OverlayWindow::UpdateHoverWindow(POINT screenPoint) {
    auto detected = WindowDetector::Detect(screenPoint, GetCurrentProcessId(), desktop_.Bounds());
    if (detected != hoverWindow_) {
        hoverWindow_ = detected;
        InvalidateRect(window_, nullptr, FALSE);
    }
}

void OverlayWindow::OnLeftButtonDown(POINT clientPoint) {
    SetFocus(window_);
    SetCapture(window_);
    const POINT screenPoint = ClientToScreenPoint(clientPoint);
    lastMouseScreen_ = screenPoint;
    if (selection_) {
        const ToolbarAction action = toolbar_.HitTest(clientPoint);
        if (action != ToolbarAction::None) {
            HandleToolbarAction(action);
            return;
        }
    }
    if (textInput_) {
        CommitTextInput();
    }
    dragAnchor_ = screenPoint;
    dragged_ = false;
    resizeHandle_ = HitTestHandle(screenPoint);
    if (selection_ && currentTool_ != AnnotationTool::None && selection_->Contains(screenPoint)) {
        dragMode_ = DragMode::Annotating;
        BeginAnnotation(screenPoint);
    } else if (selection_ && resizeHandle_ >= 0) {
        dragMode_ = DragMode::Resizing;
        dragOriginal_ = *selection_;
    } else if (selection_ && selection_->Contains(screenPoint)) {
        dragMode_ = DragMode::Moving;
        dragOriginal_ = *selection_;
    } else {
        dragMode_ = DragMode::Creating;
        selection_.reset();
    }
}

void OverlayWindow::OnMouseMove(POINT clientPoint, WPARAM keys) {
    const POINT screenPoint = ClientToScreenPoint(clientPoint);
    lastMouseScreen_ = screenPoint;
    if (!(keys & MK_LBUTTON) || dragMode_ == DragMode::None) {
        if (selection_) {
            const ToolbarAction hover = toolbar_.HitTest(clientPoint);
            UpdateToolbarHover(hover);
            SetCursor(LoadCursorW(nullptr, hover == ToolbarAction::None ? IDC_CROSS : IDC_HAND));
            if (!trackingMouseLeave_) {
                TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window_, 0};
                trackingMouseLeave_ = TrackMouseEvent(&tracking) != FALSE;
            }
        } else {
            UpdateHoverWindow(screenPoint);
        }
        return;
    }
    UpdateToolbarHover(ToolbarAction::None);

    const int deltaX = screenPoint.x - dragAnchor_.x;
    const int deltaY = screenPoint.y - dragAnchor_.y;
    if (std::abs(deltaX) > 2 || std::abs(deltaY) > 2) {
        dragged_ = true;
    }
    if (dragMode_ == DragMode::Creating) {
        selection_ = Intersect(RectI::FromPoints(dragAnchor_, screenPoint), desktop_.Bounds());
    } else if (dragMode_ == DragMode::Moving) {
        RectI moved = dragOriginal_;
        moved.left += deltaX;
        moved.right += deltaX;
        moved.top += deltaY;
        moved.bottom += deltaY;
        selection_ = ClampInside(moved, desktop_.Bounds());
    } else if (dragMode_ == DragMode::Resizing) {
        ResizeSelection(screenPoint);
    } else if (dragMode_ == DragMode::Annotating) {
        UpdateAnnotation(ClampToSelection(screenPoint));
    }
    InvalidateRect(window_, nullptr, FALSE);
}

void OverlayWindow::UpdateToolbarHover(ToolbarAction action) {
    if (hoveredToolbarAction_ == action) {
        return;
    }
    hoveredToolbarAction_ = action;
    showToolbarTooltip_ = false;
    KillTimer(window_, kToolbarTooltipTimer);
    if (action != ToolbarAction::None) {
        SetTimer(window_, kToolbarTooltipTimer, 350, nullptr);
    }
    InvalidateRect(window_, nullptr, FALSE);
}

void OverlayWindow::OnLeftButtonUp(POINT clientPoint) {
    if (GetCapture() == window_) {
        ReleaseCapture();
    }
    const POINT screenPoint = ClientToScreenPoint(clientPoint);
    if (dragMode_ == DragMode::Creating && !dragged_) {
        UpdateHoverWindow(screenPoint);
        selection_ = hoverWindow_;
    }
    if (dragMode_ == DragMode::Annotating) {
        FinishAnnotation();
    }
    if (selection_ && (selection_->Width() < 2 || selection_->Height() < 2)) {
        selection_.reset();
    }
    dragMode_ = DragMode::None;
    InvalidateRect(window_, nullptr, FALSE);
}

void OverlayWindow::MoveSelection(int deltaX, int deltaY) {
    if (!selection_) {
        return;
    }
    RectI moved = *selection_;
    moved.left += deltaX;
    moved.right += deltaX;
    moved.top += deltaY;
    moved.bottom += deltaY;
    selection_ = ClampInside(moved, desktop_.Bounds());
    InvalidateRect(window_, nullptr, FALSE);
}

void OverlayWindow::ResizeSelection(POINT screenPoint) {
    RectI resized = dragOriginal_;
    if (resizeHandle_ == 0 || resizeHandle_ == 3 || resizeHandle_ == 5) {
        resized.left = screenPoint.x;
    }
    if (resizeHandle_ == 2 || resizeHandle_ == 4 || resizeHandle_ == 7) {
        resized.right = screenPoint.x;
    }
    if (resizeHandle_ == 0 || resizeHandle_ == 1 || resizeHandle_ == 2) {
        resized.top = screenPoint.y;
    }
    if (resizeHandle_ == 5 || resizeHandle_ == 6 || resizeHandle_ == 7) {
        resized.bottom = screenPoint.y;
    }
    resized = Intersect(resized, desktop_.Bounds());
    if (resized.Width() >= 2 && resized.Height() >= 2) {
        selection_ = resized;
    }
}

int OverlayWindow::HitTestHandle(POINT point) const {
    if (!selection_) {
        return -1;
    }
    const int radius = DpiScaled(window_, 6);
    const int centerX = (selection_->left + selection_->right) / 2;
    const int centerY = (selection_->top + selection_->bottom) / 2;
    const POINT handles[8] = {
        {selection_->left, selection_->top}, {centerX, selection_->top},
        {selection_->right, selection_->top}, {selection_->left, centerY},
        {selection_->right, centerY}, {selection_->left, selection_->bottom},
        {centerX, selection_->bottom}, {selection_->right, selection_->bottom},
    };
    for (int index = 0; index < 8; ++index) {
        if (std::abs(point.x - handles[index].x) <= radius &&
            std::abs(point.y - handles[index].y) <= radius) {
            return index;
        }
    }
    return -1;
}

void OverlayWindow::DrawSelection(HDC dc, RectI screenRect, bool selected) {
    RectI rect = ScreenToClientRect(screenRect);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    UniquePen lightEdge(CreatePen(PS_SOLID, DpiScaled(window_, 3), RGB(235, 242, 250)));
    {
        SelectObjectGuard selectedLightEdge(dc, lightEdge.get());
        Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
    }
    UniquePen pen(CreatePen(PS_SOLID, DpiScaled(window_, 1), kSelectionColor));
    SelectObjectGuard selectedPen(dc, pen.get());
    Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dc, oldBrush);

    if (!selected) {
        return;
    }
    const int half = DpiScaled(window_, 4);
    const int centerX = (rect.left + rect.right) / 2;
    const int centerY = (rect.top + rect.bottom) / 2;
    const POINT handles[8] = {
        {rect.left, rect.top}, {centerX, rect.top}, {rect.right, rect.top},
        {rect.left, centerY}, {rect.right, centerY}, {rect.left, rect.bottom},
        {centerX, rect.bottom}, {rect.right, rect.bottom},
    };
    UniqueBrush brush(CreateSolidBrush(kSelectionColor));
    SelectObjectGuard selectedBrush(dc, brush.get());
    UniquePen handleBorder(CreatePen(PS_SOLID, DpiScaled(window_, 1), RGB(255, 255, 255)));
    SelectObjectGuard selectedHandleBorder(dc, handleBorder.get());
    for (POINT handle : handles) {
        Ellipse(dc, handle.x - half, handle.y - half,
                handle.x + half + 1, handle.y + half + 1);
    }
}

void OverlayWindow::DrawDimensionLabel(HDC dc, RectI screenRect) {
    const std::wstring text = std::to_wstring(screenRect.Width()) + L" × " +
                              std::to_wstring(screenRect.Height());
    RectI client = ScreenToClientRect(screenRect);
    UniqueFont font(CreateFontW(-DpiScaled(window_, 14), 0, 0, 0, FW_MEDIUM,
                                FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Segoe UI"));
    SelectObjectGuard selectedFont(dc, font.get());
    SIZE textSize{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &textSize);
    const int width = textSize.cx + DpiScaled(window_, 18);
    const int height = textSize.cy + DpiScaled(window_, 10);
    int x = client.left;
    int y = client.top - height - DpiScaled(window_, 6);
    if (y < 0) {
        y = client.top + DpiScaled(window_, 6);
    }
    x = std::clamp(x, 0, std::max(0, desktop_.width - width));
    RECT label{x, y, x + width, y + height};
    UniqueBrush shadow(CreateSolidBrush(RGB(18, 19, 22)));
    SelectObjectGuard selectedShadow(dc, shadow.get());
    UniquePen shadowPen(CreatePen(PS_SOLID, 1, RGB(18, 19, 22)));
    SelectObjectGuard selectedShadowPen(dc, shadowPen.get());
    RoundRect(dc, label.left + 2, label.top + 3,
              label.right + 2, label.bottom + 3, 8, 8);
    UniqueBrush background(CreateSolidBrush(RGB(32, 35, 40)));
    SelectObjectGuard selectedBackground(dc, background.get());
    UniquePen backgroundPen(CreatePen(PS_SOLID, 1, RGB(75, 79, 87)));
    SelectObjectGuard selectedBackgroundPen(dc, backgroundPen.get());
    RoundRect(dc, label.left, label.top, label.right, label.bottom, 8, 8);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(245, 247, 250));
    DrawTextW(dc, text.c_str(), -1, &label, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void OverlayWindow::DrawAnnotations(HDC dc) {
    if (!selection_) {
        return;
    }
    const RectI clip = ScreenToClientRect(*selection_);
    const int saved = SaveDC(dc);
    IntersectClipRect(dc, clip.left, clip.top, clip.right, clip.bottom);
    RenderContext context{
        dc,
        nullptr,
        desktop_.width,
        desktop_.height,
        desktop_.stride,
        -desktop_.originX,
        -desktop_.originY,
        true,
    };
    for (const auto& annotation : annotations_.Items()) {
        annotation->Draw(context);
    }
    if (activeAnnotation_) {
        activeAnnotation_->Draw(context);
    }
    if (selectedAnnotation_ && *selectedAnnotation_ < annotations_.Items().size()) {
        RectI bounds = ScreenToClientRect(annotations_.Items()[*selectedAnnotation_]->Bounds());
        UniquePen pen(CreatePen(PS_DOT, 1, RGB(32, 137, 255)));
        SelectObjectGuard selectedPen(dc, pen.get());
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, bounds.left - 3, bounds.top - 3, bounds.right + 3, bounds.bottom + 3);
        SelectObject(dc, oldBrush);
    }
    RestoreDC(dc, saved);
}

void OverlayWindow::DrawMagnifier(HDC dc) {
    const int size = DpiScaled(window_, 112);
    const int sourceSize = 14;
    POINT cursorClient{lastMouseScreen_.x - desktop_.originX, lastMouseScreen_.y - desktop_.originY};
    int left = cursorClient.x + DpiScaled(window_, 20);
    int top = cursorClient.y + DpiScaled(window_, 20);
    if (left + size > desktop_.width) {
        left = cursorClient.x - size - DpiScaled(window_, 20);
    }
    if (top + size > desktop_.height) {
        top = cursorClient.y - size - DpiScaled(window_, 20);
    }
    left = std::clamp(left, 0, std::max(0, desktop_.width - size));
    top = std::clamp(top, 0, std::max(0, desktop_.height - size));
    const int sourceX = std::clamp(static_cast<int>(cursorClient.x) - sourceSize / 2, 0,
                                   std::max(0, desktop_.width - sourceSize));
    const int sourceY = std::clamp(static_cast<int>(cursorClient.y) - sourceSize / 2, 0,
                                   std::max(0, desktop_.height - sourceSize));
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchBlt(dc, left, top, size, size, originalDc_, sourceX, sourceY,
               sourceSize, sourceSize, SRCCOPY);
    UniquePen border(CreatePen(PS_SOLID, 2, RGB(255, 255, 255)));
    SelectObjectGuard selectedPen(dc, border.get());
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, left, top, left + size, top + size);
    MoveToEx(dc, left + size / 2, top + size / 2 - 8, nullptr);
    LineTo(dc, left + size / 2, top + size / 2 + 9);
    MoveToEx(dc, left + size / 2 - 8, top + size / 2, nullptr);
    LineTo(dc, left + size / 2 + 9, top + size / 2);
    SelectObject(dc, oldBrush);
}

void OverlayWindow::HandleToolbarAction(ToolbarAction action) {
    switch (action) {
    case ToolbarAction::Rectangle:
        currentTool_ = AnnotationTool::Rectangle;
        break;
    case ToolbarAction::Arrow:
        currentTool_ = AnnotationTool::Arrow;
        break;
    case ToolbarAction::Pen:
        currentTool_ = AnnotationTool::Pen;
        break;
    case ToolbarAction::Text:
        currentTool_ = AnnotationTool::Text;
        break;
    case ToolbarAction::Mosaic:
        currentTool_ = AnnotationTool::Mosaic;
        break;
    case ToolbarAction::Undo:
        annotations_.Undo();
        selectedAnnotation_.reset();
        break;
    case ToolbarAction::Redo:
        annotations_.Redo();
        selectedAnnotation_.reset();
        break;
    case ToolbarAction::Pin:
        PinAndFinish();
        return;
    case ToolbarAction::Save:
        SaveAndFinish();
        return;
    case ToolbarAction::Copy:
        CopyAndFinish();
        return;
    case ToolbarAction::Cancel:
        Cancel();
        return;
    default:
        break;
    }
    InvalidateRect(window_, nullptr, FALSE);
}

void OverlayWindow::BeginAnnotation(POINT screenPoint) {
    annotationStart_ = screenPoint;
    if (currentTool_ == AnnotationTool::Rectangle) {
        activeAnnotation_ = std::make_shared<RectangleAnnotation>(screenPoint, screenPoint);
    } else if (currentTool_ == AnnotationTool::Arrow) {
        activeAnnotation_ = std::make_shared<ArrowAnnotation>(screenPoint, screenPoint);
    } else if (currentTool_ == AnnotationTool::Pen) {
        activeAnnotation_ = std::make_shared<PenAnnotation>(screenPoint);
    } else if (currentTool_ == AnnotationTool::Mosaic) {
        activeAnnotation_ = std::make_shared<MosaicAnnotation>(screenPoint, screenPoint);
    } else if (currentTool_ == AnnotationTool::Text) {
        dragMode_ = DragMode::None;
        if (GetCapture() == window_) {
            ReleaseCapture();
        }
        BeginTextInput(screenPoint);
    }
}

void OverlayWindow::UpdateAnnotation(POINT screenPoint) {
    if (currentTool_ == AnnotationTool::Rectangle) {
        activeAnnotation_ = std::make_shared<RectangleAnnotation>(annotationStart_, screenPoint);
    } else if (currentTool_ == AnnotationTool::Arrow) {
        activeAnnotation_ = std::make_shared<ArrowAnnotation>(annotationStart_, screenPoint);
    } else if (currentTool_ == AnnotationTool::Pen) {
        auto pen = std::dynamic_pointer_cast<PenAnnotation>(activeAnnotation_);
        if (pen) {
            pen->AddPoint(screenPoint);
        }
    } else if (currentTool_ == AnnotationTool::Mosaic) {
        activeAnnotation_ = std::make_shared<MosaicAnnotation>(annotationStart_, screenPoint);
    }
}

void OverlayWindow::FinishAnnotation() {
    if (!activeAnnotation_) {
        return;
    }
    const RectI bounds = activeAnnotation_->Bounds();
    if (!bounds.Empty()) {
        annotations_.Add(activeAnnotation_);
        selectedAnnotation_ = annotations_.Items().size() - 1;
    }
    activeAnnotation_.reset();
}

void OverlayWindow::BeginTextInput(POINT screenPoint) {
    if (!selection_) {
        return;
    }
    textPosition_ = screenPoint;
    POINT client{screenPoint.x - desktop_.originX, screenPoint.y - desktop_.originY};
    const RectI selectionClient = ScreenToClientRect(*selection_);
    const int width = std::min(DpiScaled(window_, 260),
                               selectionClient.right - static_cast<int>(client.x));
    const int height = std::min(DpiScaled(window_, 90),
                                selectionClient.bottom - static_cast<int>(client.y));
    textInput_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_WANTRETURN,
                                 client.x, client.y, std::max(width, 80), std::max(height, 32),
                                 window_, nullptr, instance_, nullptr);
    if (!textInput_) {
        return;
    }
    SetWindowSubclass(textInput_, TextEditSubclassProc, 1,
                      reinterpret_cast<DWORD_PTR>(this));
    textInputFont_ = CreateFontW(-DpiScaled(window_, 22), 0, 0, 0, FW_SEMIBOLD,
                                 FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessageW(textInput_, WM_SETFONT, reinterpret_cast<WPARAM>(textInputFont_), TRUE);
    SetFocus(textInput_);
}

void OverlayWindow::CommitTextInput() {
    if (!textInput_) {
        return;
    }
    const int length = GetWindowTextLengthW(textInput_);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(textInput_, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    HWND edit = textInput_;
    textInput_ = nullptr;
    DestroyWindow(edit);
    if (textInputFont_) {
        DeleteObject(textInputFont_);
        textInputFont_ = nullptr;
    }
    if (!text.empty()) {
        annotations_.Add(std::make_shared<TextAnnotation>(textPosition_, std::move(text)));
        selectedAnnotation_ = annotations_.Items().size() - 1;
    }
    SetFocus(window_);
    InvalidateRect(window_, nullptr, FALSE);
}

void OverlayWindow::CancelTextInput() {
    if (!textInput_) {
        return;
    }
    HWND edit = textInput_;
    textInput_ = nullptr;
    DestroyWindow(edit);
    if (textInputFont_) {
        DeleteObject(textInputFont_);
        textInputFont_ = nullptr;
    }
    SetFocus(window_);
}

POINT OverlayWindow::ClampToSelection(POINT point) const {
    if (!selection_) {
        return point;
    }
    point.x = static_cast<LONG>(std::clamp(static_cast<int>(point.x), selection_->left,
                                           selection_->right - 1));
    point.y = static_cast<LONG>(std::clamp(static_cast<int>(point.y), selection_->top,
                                           selection_->bottom - 1));
    return point;
}

UINT OverlayWindow::CurrentDpi() const {
    return GetDpiForWindow(window_);
}

void OverlayWindow::CopyAndFinish() {
    if (!selection_) {
        return;
    }
    CommitTextInput();
    ImageData output = ImageComposer::Compose(desktop_, *selection_, annotations_.Items());
    if (!ClipboardExporter::Copy(window_, output)) {
        MessageBoxW(window_, L"无法写入剪贴板，请稍后重试。截图仍保持打开。",
                    L"YanSnap", MB_OK | MB_ICONERROR);
        return;
    }
    if (settings_.defaultAction == DefaultAction::CopyAndAutoSave) {
        std::filesystem::path directory =
            settings_.saveDirectory.empty() ? PngEncoder::DefaultScreenshotsDirectory()
                                            : settings_.saveDirectory;
        std::error_code directoryError;
        std::filesystem::create_directories(directory, directoryError);
        if (directoryError) {
            MessageBoxW(window_, L"已复制截图，但无法创建自动保存目录。截图仍保持打开。",
                        L"YanSnap", MB_OK | MB_ICONERROR);
            return;
        }
        const std::filesystem::path path =
            directory / PngEncoder::SuggestedFileName(directory.wstring());
        BeginAsyncSave(path.wstring(), std::move(output), true);
        return;
    }
    if (tray_ && settings_.showNotifications) {
        const std::wstring message = L"截图已复制，" + std::to_wstring(output.width) + L" × " +
                                     std::to_wstring(output.height);
        tray_->Notify(L"YanSnap", message.c_str());
    }
    Finish();
}

void OverlayWindow::SaveAndFinish() {
    if (!selection_) {
        return;
    }
    const std::wstring directory = settings_.saveDirectory.empty()
                                       ? PngEncoder::DefaultScreenshotsDirectory()
                                       : settings_.saveDirectory;
    std::error_code directoryError;
    std::filesystem::create_directories(directory, directoryError);
    if (directoryError) {
        MessageBoxW(window_, L"无法创建保存目录，请在设置中更换目录。",
                    L"YanSnap", MB_OK | MB_ICONERROR);
        return;
    }
    std::wstring fileName = PngEncoder::SuggestedFileName(directory);
    std::vector<wchar_t> path(32768, L'\0');
    std::filesystem::path initial = std::filesystem::path(directory) / fileName;
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

    CommitTextInput();
    ImageData output = ImageComposer::Compose(desktop_, *selection_, annotations_.Items());
    BeginAsyncSave(path.data(), std::move(output), false);
}

void OverlayWindow::PinAndFinish() {
    if (!selection_ || !pinCallback_) {
        return;
    }
    CommitTextInput();
    ImageData output = ImageComposer::Compose(desktop_, *selection_, annotations_.Items());
    if (!output.Valid()) {
        MessageBoxW(window_, L"无法生成贴图。截图仍保持打开。",
                    L"YanSnap", MB_OK | MB_ICONERROR);
        return;
    }
    const POINT position{selection_->left, selection_->top};
    pinCallback_(std::move(output), position);
    Finish();
}

void OverlayWindow::BeginAsyncSave(std::wstring path, ImageData image, bool copied) {
    if (saving_) {
        return;
    }
    saving_ = true;
    if (saveThread_.joinable()) {
        saveThread_.join();
    }
    const HWND target = window_;
    saveThread_ = std::thread(
        [this, target, path = std::move(path), image = std::move(image), copied]() mutable {
            auto* result = new AsyncSaveResult;
            result->copied = copied;
            result->success = PngEncoder::Save(path, image, &result->error);
            pendingSaveResult_.store(result);
            if (!PostMessageW(target, kAsyncSaveCompleteMessage, 0,
                              reinterpret_cast<LPARAM>(result))) {
                void* expected = result;
                if (pendingSaveResult_.compare_exchange_strong(expected, nullptr)) {
                    delete result;
                }
            }
        });
}

void OverlayWindow::HandleAsyncSaveComplete(void* rawResult) {
    void* expected = rawResult;
    if (!pendingSaveResult_.compare_exchange_strong(expected, nullptr)) {
        return;
    }
    if (saveThread_.joinable()) {
        saveThread_.join();
    }
    saving_ = false;
    std::unique_ptr<AsyncSaveResult> result(static_cast<AsyncSaveResult*>(rawResult));
    if (!result->success) {
        MessageBoxW(window_, result->error.c_str(), L"YanSnap", MB_OK | MB_ICONERROR);
        return;
    }
    if (tray_ && settings_.showNotifications) {
        tray_->Notify(L"YanSnap", result->copied ? L"截图已复制并自动保存。" : L"截图已保存。");
    }
    Finish();
}

void OverlayWindow::Finish() {
    Cancel();
}

POINT OverlayWindow::ClientToScreenPoint(POINT clientPoint) const noexcept {
    return POINT{clientPoint.x + desktop_.originX, clientPoint.y + desktop_.originY};
}

RectI OverlayWindow::ScreenToClientRect(RectI screenRect) const noexcept {
    screenRect.left -= desktop_.originX;
    screenRect.right -= desktop_.originX;
    screenRect.top -= desktop_.originY;
    screenRect.bottom -= desktop_.originY;
    return screenRect;
}

}  // namespace snaplite
