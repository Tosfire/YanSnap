#pragma once

#include <windows.h>

#include "export/ImageData.h"

namespace snaplite {

class PinWindow {
public:
    static constexpr UINT kClosedMessage = WM_APP + 31;

    PinWindow(HINSTANCE instance, HWND owner, ImageData image, POINT screenPosition);
    ~PinWindow();

    PinWindow(const PinWindow&) = delete;
    PinWindow& operator=(const PinWindow&) = delete;

    bool Show();
    void Close();
    void SetVisible(bool visible);
    [[nodiscard]] bool IsVisible() const;
    [[nodiscard]] HWND Handle() const noexcept { return window_; }

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass();
    bool CreateImageSurface();
    void DestroyImageSurface();
    void Paint();
    void BeginDrag();
    void ContinueDrag();
    void EndDrag();
    void ZoomAt(POINT screenPoint, double factor);
    void ResetSize();
    void AdjustOpacity(int delta);
    void ShowContextMenu(POINT screenPoint);
    void SaveToFile();
    void ClampToWorkingArea(int& left, int& top, int width, int height) const;

    HINSTANCE instance_{};
    HWND owner_{};
    HWND window_{};
    ImageData image_;
    POINT initialPosition_{};
    HDC imageDc_{};
    HBITMAP imageBitmap_{};
    HGDIOBJ oldImageBitmap_{};
    double scale_{1.0};
    BYTE opacity_{255};
    bool dragging_{};
    bool hovered_{};
    bool trackingMouseLeave_{};
    POINT dragCursorStart_{};
    POINT dragWindowStart_{};
};

}  // namespace snaplite

