#pragma once

#include <windows.h>

#include <atomic>
#include <thread>

#include "common/Geometry.h"
#include "common/Win32.h"
#include "export/ImageData.h"
#include "ocr/OcrRecognizer.h"

namespace snaplite {

class OcrPanel {
public:
    explicit OcrPanel(HINSTANCE instance);
    ~OcrPanel();

    OcrPanel(const OcrPanel&) = delete;
    OcrPanel& operator=(const OcrPanel&) = delete;

    bool Show(HWND parent, RectI selectionClient, RectI desktopClient,
              ImageData image);
    void Hide();
    void CopyAll();
    [[nodiscard]] bool Visible() const noexcept { return window_ != nullptr; }
    [[nodiscard]] bool Busy() const noexcept { return !workerFinished_.load(); }

private:
    static constexpr UINT kRecognitionCompleteMessage = WM_APP + 40;

    static LRESULT CALLBACK WindowProc(HWND window, UINT message,
                                       WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditSubclassProc(HWND window, UINT message,
                                              WPARAM wParam, LPARAM lParam,
                                              UINT_PTR subclassId,
                                              DWORD_PTR referenceData);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool RegisterWindowClass();
    RectI CalculateBounds(RectI selectionClient, RectI desktopClient) const;
    void CreateControls();
    void LayoutControls();
    void Paint();
    void DrawButton(const DRAWITEMSTRUCT& draw) const;
    void StartRecognition(ImageData image);
    void HandleRecognitionComplete(void* rawResult);
    void ClosePanel();
    [[nodiscard]] int Scale(int value) const;

    HINSTANCE instance_{};
    HWND parent_{};
    HWND window_{};
    HWND statusText_{};
    HWND resultEdit_{};
    HWND hintText_{};
    HWND copyButton_{};
    HWND closeButton_{};
    UINT dpi_{96};
    UniqueBrush pageBrush_;
    UniqueBrush editBrush_;
    UniqueFont normalFont_;
    UniqueFont statusFont_;
    UniqueFont buttonFont_;
    std::thread worker_;
    std::atomic<bool> workerFinished_{true};
    std::atomic<OcrRecognitionResult*> pendingResult_{nullptr};
};

}  // namespace snaplite
