#pragma once

#include <windows.h>

#include <atomic>
#include <functional>
#include <optional>
#include <memory>
#include <string>
#include <thread>

#include "annotation/UndoStack.h"
#include "capture/DesktopCapture.h"
#include "common/Geometry.h"
#include "common/Win32.h"
#include "export/ImageData.h"
#include "overlay/Toolbar.h"
#include "settings/Settings.h"

namespace snaplite {

class TrayIcon;

class OverlayWindow {
public:
    using PinCallback = std::function<void(ImageData, POINT)>;

    static constexpr UINT kSessionEndedMessage = WM_APP + 21;
    static constexpr UINT kAsyncSaveCompleteMessage = WM_APP + 22;

    OverlayWindow(HINSTANCE instance, HWND owner, TrayIcon* tray, Settings settings,
                  PinCallback pinCallback = {});
    ~OverlayWindow();

    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    bool Start(DesktopImage image);
    void Cancel();
    [[nodiscard]] bool Active() const noexcept { return window_ != nullptr; }
    [[nodiscard]] DWORD LastErrorCode() const noexcept { return lastErrorCode_; }

private:
    enum class DragMode {
        None,
        Creating,
        Moving,
        Resizing,
        Annotating,
    };

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK TextEditSubclassProc(HWND window, UINT message, WPARAM wParam,
                                                 LPARAM lParam, UINT_PTR subclassId,
                                                 DWORD_PTR referenceData);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass();
    bool CreatePaintSurfaces();
    void DestroyPaintSurfaces();
    void Paint();
    void UpdateHoverWindow(POINT screenPoint);
    void OnLeftButtonDown(POINT clientPoint);
    void OnMouseMove(POINT clientPoint, WPARAM keys);
    void OnLeftButtonUp(POINT clientPoint);
    void MoveSelection(int deltaX, int deltaY);
    void ResizeSelection(POINT screenPoint);
    int HitTestHandle(POINT screenPoint) const;
    void DrawSelection(HDC dc, RectI screenRect, bool selected);
    void DrawDimensionLabel(HDC dc, RectI screenRect);
    void DrawAnnotations(HDC dc);
    void DrawMagnifier(HDC dc);
    void UpdateToolbarHover(ToolbarAction action);
    void HandleToolbarAction(ToolbarAction action);
    void BeginAnnotation(POINT screenPoint);
    void UpdateAnnotation(POINT screenPoint);
    void FinishAnnotation();
    void BeginTextInput(POINT screenPoint);
    void CommitTextInput();
    void CancelTextInput();
    POINT ClampToSelection(POINT screenPoint) const;
    UINT CurrentDpi() const;
    void CopyAndFinish();
    void SaveAndFinish();
    void PinAndFinish();
    void BeginAsyncSave(std::wstring path, ImageData image, bool copied);
    void HandleAsyncSaveComplete(void* result);
    void Finish();
    POINT ClientToScreenPoint(POINT clientPoint) const noexcept;
    RectI ScreenToClientRect(RectI screenRect) const noexcept;

    HINSTANCE instance_{};
    HWND owner_{};
    TrayIcon* tray_{};
    Settings settings_;
    PinCallback pinCallback_;
    HWND window_{};
    DesktopImage desktop_;
    HDC originalDc_{};
    HDC dimmedDc_{};
    HDC backBufferDc_{};
    HBITMAP originalBitmap_{};
    HBITMAP dimmedBitmap_{};
    HBITMAP backBufferBitmap_{};
    HGDIOBJ oldOriginalBitmap_{};
    HGDIOBJ oldDimmedBitmap_{};
    HGDIOBJ oldBackBufferBitmap_{};
    std::optional<RectI> selection_;
    std::optional<RectI> hoverWindow_;
    DragMode dragMode_{DragMode::None};
    POINT dragAnchor_{};
    RectI dragOriginal_{};
    int resizeHandle_{-1};
    bool dragged_{};
    DWORD lastErrorCode_{};
    Toolbar toolbar_;
    AnnotationTool currentTool_{AnnotationTool::None};
    UndoStack annotations_;
    AnnotationPtr activeAnnotation_;
    std::optional<std::size_t> selectedAnnotation_;
    POINT annotationStart_{};
    POINT lastMouseScreen_{};
    HWND textInput_{};
    HFONT textInputFont_{};
    POINT textPosition_{};
    ToolbarAction hoveredToolbarAction_{ToolbarAction::None};
    bool showToolbarTooltip_{};
    bool trackingMouseLeave_{};
    bool saving_{};
    std::thread saveThread_;
    std::atomic<void*> pendingSaveResult_{nullptr};
};

}  // namespace snaplite
