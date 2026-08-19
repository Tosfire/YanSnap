#include "ocr/OcrPanel.h"

#include <commctrl.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "export/ClipboardExporter.h"

namespace snaplite {

namespace {

constexpr wchar_t kWindowClassName[] = L"YanSnap.OcrPanel.v1";
constexpr int kCopyButton = 4201;
constexpr int kCloseButton = 4202;

constexpr COLORREF kPageColor = RGB(247, 249, 253);
constexpr COLORREF kEditColor = RGB(255, 255, 255);
constexpr COLORREF kTextColor = RGB(29, 33, 41);
constexpr COLORREF kMutedTextColor = RGB(92, 100, 113);
constexpr COLORREF kBorderColor = RGB(211, 218, 229);
constexpr COLORREF kAccentColor = RGB(31, 117, 235);

std::wstring ControlText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring result(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(control, result.data(), length + 1);
    result.resize(static_cast<std::size_t>(length));
    return result;
}

}  // namespace

OcrPanel::OcrPanel(HINSTANCE instance) : instance_(instance) {}

OcrPanel::~OcrPanel() {
    Hide();
    if (worker_.joinable()) {
        worker_.join();
    }
    delete pendingResult_.exchange(nullptr);
}

int OcrPanel::Scale(int value) const {
    return MulDiv(value, static_cast<int>(dpi_), 96);
}

bool OcrPanel::RegisterWindowClass() {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    return RegisterClassExW(&windowClass) != 0 ||
           GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

RectI OcrPanel::CalculateBounds(RectI selectionClient,
                                RectI desktopClient) const {
    const int margin = Scale(16);
    const int gap = Scale(12);
    const int availableWidth = std::max(1, desktopClient.Width() - margin * 2);
    const int availableHeight = std::max(1, desktopClient.Height() - margin * 2);
    const int width = std::min(Scale(500), availableWidth);
    const int height = std::min(Scale(380), availableHeight);

    int left = selectionClient.right + gap;
    if (left + width > desktopClient.right - margin) {
        left = selectionClient.left - gap - width;
    }
    if (left < desktopClient.left + margin) {
        left = std::clamp(selectionClient.left,
                          desktopClient.left + margin,
                          std::max(desktopClient.left + margin,
                                   desktopClient.right - margin - width));
    }
    int top = std::clamp(selectionClient.top,
                         desktopClient.top + margin,
                         std::max(desktopClient.top + margin,
                                  desktopClient.bottom - margin - height));
    return RectI{left, top, left + width, top + height};
}

bool OcrPanel::Show(HWND parent, RectI selectionClient, RectI desktopClient,
                    ImageData image) {
    if (!parent || !image.Valid()) {
        return false;
    }
    if (window_) {
        SetFocus(resultEdit_ ? resultEdit_ : window_);
        return true;
    }
    if (worker_.joinable()) {
        if (!workerFinished_.load()) {
            return false;
        }
        worker_.join();
    }
    delete pendingResult_.exchange(nullptr);

    parent_ = parent;
    dpi_ = GetDpiForWindow(parent);
    pageBrush_.reset(CreateSolidBrush(kPageColor));
    editBrush_.reset(CreateSolidBrush(kEditColor));
    normalFont_.reset(CreateFontW(
        -MulDiv(10, static_cast<int>(dpi_), 72), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"));
    statusFont_.reset(CreateFontW(
        -MulDiv(11, static_cast<int>(dpi_), 72), 0, 0, 0, FW_SEMIBOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"));
    buttonFont_.reset(CreateFontW(
        -MulDiv(10, static_cast<int>(dpi_), 72), 0, 0, 0, FW_SEMIBOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"));

    if (!RegisterWindowClass()) {
        return false;
    }
    const RectI bounds = CalculateBounds(selectionClient, desktopClient);
    window_ = CreateWindowExW(
        WS_EX_CONTROLPARENT, kWindowClassName, L"文字识别",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        bounds.left, bounds.top, bounds.Width(), bounds.Height(),
        parent, nullptr, instance_, this);
    if (!window_) {
        return false;
    }
    HRGN region = CreateRoundRectRgn(
        0, 0, bounds.Width() + 1, bounds.Height() + 1,
        Scale(14), Scale(14));
    if (region && SetWindowRgn(window_, region, TRUE) == 0) {
        DeleteObject(region);
    }
    StartRecognition(std::move(image));
    SetFocus(resultEdit_);
    return true;
}

void OcrPanel::Hide() {
    if (window_) {
        DestroyWindow(window_);
    }
}

void OcrPanel::ClosePanel() {
    HWND parent = parent_;
    Hide();
    if (parent) {
        SetFocus(parent);
        InvalidateRect(parent, nullptr, FALSE);
    }
}

LRESULT CALLBACK OcrPanel::WindowProc(HWND window, UINT message,
                                       WPARAM wParam, LPARAM lParam) {
    auto* panel = reinterpret_cast<OcrPanel*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        panel = static_cast<OcrPanel*>(create->lpCreateParams);
        panel->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(panel));
    }
    if (!panel) {
        return DefWindowProcW(window, message, wParam, lParam);
    }
    const LRESULT result = panel->HandleMessage(message, wParam, lParam);
    if (message == WM_NCDESTROY) {
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        panel->window_ = nullptr;
    }
    return result;
}

LRESULT CALLBACK OcrPanel::EditSubclassProc(HWND window, UINT message,
                                             WPARAM wParam, LPARAM lParam,
                                             UINT_PTR subclassId,
                                             DWORD_PTR referenceData) {
    auto* panel = reinterpret_cast<OcrPanel*>(referenceData);
    if (message == WM_KEYDOWN && wParam == VK_ESCAPE) {
        panel->ClosePanel();
        return 0;
    }
    if (message == WM_KEYDOWN && wParam == 'C' &&
        (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
        window != panel->resultEdit_) {
        panel->CopyAll();
        return 0;
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(window, EditSubclassProc, subclassId);
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

LRESULT OcrPanel::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateControls();
        return 0;
    case WM_SIZE:
        LayoutControls();
        return 0;
    case WM_PAINT:
        Paint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_DRAWITEM:
        DrawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        const HWND control = reinterpret_cast<HWND>(lParam);
        SetTextColor(dc, control == hintText_ ? kMutedTextColor : kTextColor);
        SetBkColor(dc, kPageColor);
        return reinterpret_cast<LRESULT>(pageBrush_.get());
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, kTextColor);
        SetBkColor(dc, kEditColor);
        return reinterpret_cast<LRESULT>(editBrush_.get());
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kCopyButton) {
            CopyAll();
        } else if (LOWORD(wParam) == kCloseButton) {
            ClosePanel();
        }
        return 0;
    case kRecognitionCompleteMessage:
        HandleRecognitionComplete(reinterpret_cast<void*>(lParam));
        return 0;
    case WM_DESTROY:
        statusText_ = nullptr;
        resultEdit_ = nullptr;
        hintText_ = nullptr;
        copyButton_ = nullptr;
        closeButton_ = nullptr;
        delete pendingResult_.exchange(nullptr);
        return 0;
    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
}

void OcrPanel::CreateControls() {
    const auto create = [this](DWORD extendedStyle, const wchar_t* className,
                               const wchar_t* text, DWORD style, int id,
                               HFONT font) {
        HWND control = CreateWindowExW(
            extendedStyle, className, text, WS_CHILD | WS_VISIBLE | style,
            0, 0, 0, 0, window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return control;
    };

    statusText_ = create(0, L"STATIC", L"正在识别中文、英文、数字和网址…",
                         SS_CENTERIMAGE, 0, statusFont_.get());
    resultEdit_ = create(
        WS_EX_CLIENTEDGE, L"EDIT", L"正在使用 Windows 本地 OCR 识别，请稍候…",
        ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL |
            WS_VSCROLL | WS_TABSTOP,
        0, normalFont_.get());
    SendMessageW(resultEdit_, EM_SETREADONLY, TRUE, 0);
    SetWindowSubclass(resultEdit_, EditSubclassProc, 1,
                      reinterpret_cast<DWORD_PTR>(this));
    hintText_ = create(0, L"STATIC",
                       L"结果可编辑 · Ctrl+A 全选 · Ctrl+C 复制 · 全程本地识别",
                       SS_CENTERIMAGE, 0, normalFont_.get());
    copyButton_ = create(0, L"BUTTON", L"复制全部",
                         BS_OWNERDRAW | WS_TABSTOP, kCopyButton,
                         buttonFont_.get());
    closeButton_ = create(0, L"BUTTON", L"返回截图",
                          BS_OWNERDRAW | WS_TABSTOP, kCloseButton,
                          buttonFont_.get());
    SetWindowSubclass(copyButton_, EditSubclassProc, 1,
                      reinterpret_cast<DWORD_PTR>(this));
    SetWindowSubclass(closeButton_, EditSubclassProc, 1,
                      reinterpret_cast<DWORD_PTR>(this));
    EnableWindow(copyButton_, FALSE);
    LayoutControls();
}

void OcrPanel::LayoutControls() {
    if (!window_ || !statusText_) {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    const int width = static_cast<int>(client.right);
    const int bottom = static_cast<int>(client.bottom);
    const int margin = Scale(22);
    const int statusHeight = Scale(28);
    const int hintHeight = Scale(23);
    const int buttonHeight = Scale(36);
    const int copyWidth = Scale(104);
    const int closeWidth = Scale(96);
    const int gap = Scale(10);

    MoveWindow(statusText_, margin, Scale(15), width - margin * 2,
               statusHeight, TRUE);
    const int editTop = Scale(52);
    const int buttonTop = bottom - margin - buttonHeight;
    const int hintTop = buttonTop - hintHeight - Scale(9);
    MoveWindow(resultEdit_, margin, editTop, width - margin * 2,
               std::max(Scale(120), hintTop - editTop - Scale(5)), TRUE);
    MoveWindow(hintText_, margin, hintTop,
               std::max(Scale(140), width - margin * 2 -
                                        copyWidth - closeWidth - gap * 2),
               hintHeight, TRUE);
    MoveWindow(closeButton_, width - margin - closeWidth, buttonTop,
               closeWidth, buttonHeight, TRUE);
    MoveWindow(copyButton_, width - margin - closeWidth - gap - copyWidth,
               buttonTop, copyWidth, buttonHeight, TRUE);
}

void OcrPanel::Paint() {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window_, &paint);
    RECT client{};
    GetClientRect(window_, &client);
    FillRect(dc, &client, pageBrush_.get());
    UniquePen border(CreatePen(PS_SOLID, 1, kBorderColor));
    SelectObjectGuard selectedBorder(dc, border.get());
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(dc, client.left, client.top, client.right - 1,
              client.bottom - 1, Scale(14), Scale(14));
    SelectObject(dc, oldBrush);
    EndPaint(window_, &paint);
}

void OcrPanel::DrawButton(const DRAWITEMSTRUCT& draw) const {
    const bool pressed = (draw.itemState & ODS_SELECTED) != 0;
    const bool disabled = (draw.itemState & ODS_DISABLED) != 0;
    const bool primary = draw.CtlID == kCopyButton;

    COLORREF background = kEditColor;
    COLORREF border = kBorderColor;
    COLORREF text = kTextColor;
    if (disabled) {
        background = RGB(228, 232, 239);
        border = RGB(215, 220, 229);
        text = RGB(148, 155, 168);
    } else if (primary) {
        background = pressed ? RGB(22, 91, 190) : kAccentColor;
        border = background;
        text = RGB(255, 255, 255);
    } else if (pressed) {
        background = RGB(235, 238, 244);
    }

    UniqueBrush brush(CreateSolidBrush(background));
    UniquePen pen(CreatePen(PS_SOLID, 1, border));
    SelectObjectGuard selectedBrush(draw.hDC, brush.get());
    SelectObjectGuard selectedPen(draw.hDC, pen.get());
    RoundRect(draw.hDC, draw.rcItem.left, draw.rcItem.top,
              draw.rcItem.right, draw.rcItem.bottom, Scale(9), Scale(9));

    wchar_t textBuffer[64]{};
    GetWindowTextW(draw.hwndItem, textBuffer, ARRAYSIZE(textBuffer));
    SelectObjectGuard selectedFont(draw.hDC, buttonFont_.get());
    SetBkMode(draw.hDC, TRANSPARENT);
    SetTextColor(draw.hDC, text);
    RECT textRect = draw.rcItem;
    DrawTextW(draw.hDC, textBuffer, -1, &textRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void OcrPanel::StartRecognition(ImageData image) {
    workerFinished_.store(false);
    const HWND target = window_;
    worker_ = std::thread([this, target, image = std::move(image)]() mutable {
        auto* result = new OcrRecognitionResult(OcrRecognizer::Recognize(image));
        pendingResult_.store(result);
        workerFinished_.store(true);
        if (!PostMessageW(target, kRecognitionCompleteMessage, 0,
                          reinterpret_cast<LPARAM>(result))) {
            OcrRecognitionResult* expected = result;
            if (pendingResult_.compare_exchange_strong(expected, nullptr)) {
                delete result;
            }
        }
    });
}

void OcrPanel::HandleRecognitionComplete(void* rawResult) {
    auto* resultPointer = static_cast<OcrRecognitionResult*>(rawResult);
    OcrRecognitionResult* expected = resultPointer;
    if (!pendingResult_.compare_exchange_strong(expected, nullptr)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    std::unique_ptr<OcrRecognitionResult> result(resultPointer);
    SendMessageW(resultEdit_, EM_SETREADONLY, FALSE, 0);

    if (!result->success) {
        SetWindowTextW(statusText_, L"文字识别失败");
        SetWindowTextW(resultEdit_, result->error.c_str());
        EnableWindow(copyButton_, FALSE);
        return;
    }
    if (result->text.empty()) {
        SetWindowTextW(statusText_, L"未识别到文字");
        SetWindowTextW(resultEdit_,
                       L"没有识别到可复制的文字。请返回截图后扩大选区，或选择更清晰的文字区域。");
        EnableWindow(copyButton_, FALSE);
        return;
    }

    const std::wstring status = L"识别完成 · " + result->languageTag + L" · " +
        std::to_wstring(result->text.size()) + L" 个字符";
    SetWindowTextW(statusText_, status.c_str());
    SetWindowTextW(resultEdit_, result->text.c_str());
    EnableWindow(copyButton_, TRUE);
    SendMessageW(resultEdit_, EM_SETSEL, 0, -1);
    SetFocus(resultEdit_);
}

void OcrPanel::CopyAll() {
    if (!window_ || !resultEdit_ || !IsWindowEnabled(copyButton_)) {
        return;
    }
    const std::wstring text = ControlText(resultEdit_);
    if (text.empty()) {
        return;
    }
    if (!ClipboardExporter::CopyText(window_, text)) {
        SetWindowTextW(statusText_, L"复制失败，请稍后重试");
        return;
    }
    SetWindowTextW(statusText_, L"已复制到剪贴板，可直接粘贴");
    SendMessageW(resultEdit_, EM_SETSEL, 0, -1);
    SetFocus(resultEdit_);
}

}  // namespace snaplite
