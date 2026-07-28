#include "settings/SettingsWindow.h"

#include <commctrl.h>
#include <shlobj.h>
#include <windowsx.h>

#include <utility>
#include <vector>

#include "common/AppIcon.h"

namespace snaplite {

namespace {

constexpr wchar_t kSettingsClassName[] = L"YanSnap.SettingsWindow.v2";
constexpr int kHotkeyEdit = 2001;
constexpr int kActionCombo = 2002;
constexpr int kDirectoryEdit = 2003;
constexpr int kBrowseButton = 2004;
constexpr int kNotificationCheck = 2005;
constexpr int kCursorCheck = 2006;
constexpr int kStartupCheck = 2007;
constexpr int kSaveButton = IDOK;
constexpr int kCancelButton = IDCANCEL;

constexpr COLORREF kPageColor = RGB(246, 248, 252);
constexpr COLORREF kCardColor = RGB(255, 255, 255);
constexpr COLORREF kTextColor = RGB(29, 33, 41);
constexpr COLORREF kMutedTextColor = RGB(78, 86, 99);
constexpr COLORREF kBorderColor = RGB(223, 228, 237);
constexpr COLORREF kAccentColor = RGB(31, 117, 235);

std::wstring ControlText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring result(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(control, result.data(), length + 1);
    result.resize(static_cast<std::size_t>(length));
    return result;
}

WORD ToHotkeyControlValue(UINT modifiers, UINT key) {
    BYTE flags = 0;
    if (modifiers & MOD_CONTROL) {
        flags |= HOTKEYF_CONTROL;
    }
    if (modifiers & MOD_SHIFT) {
        flags |= HOTKEYF_SHIFT;
    }
    if (modifiers & MOD_ALT) {
        flags |= HOTKEYF_ALT;
    }
    return MAKEWORD(static_cast<BYTE>(key), flags);
}

UINT FromHotkeyControlFlags(BYTE flags) {
    UINT modifiers = 0;
    if (flags & HOTKEYF_CONTROL) {
        modifiers |= MOD_CONTROL;
    }
    if (flags & HOTKEYF_SHIFT) {
        modifiers |= MOD_SHIFT;
    }
    if (flags & HOTKEYF_ALT) {
        modifiers |= MOD_ALT;
    }
    return modifiers;
}

}  // namespace

SettingsWindow::SettingsWindow(HINSTANCE instance, HWND owner, Settings settings,
                               SaveCallback callback)
    : instance_(instance), owner_(owner), settings_(std::move(settings)),
      callback_(std::move(callback)) {
    pageBrush_.reset(CreateSolidBrush(kPageColor));
    cardBrush_.reset(CreateSolidBrush(kCardColor));
}

SettingsWindow::~SettingsWindow() {
    if (window_) {
        DestroyWindow(window_);
    }
}

int SettingsWindow::Scale(int value) const {
    return MulDiv(value, static_cast<int>(dpi_), 96);
}

bool SettingsWindow::Show() {
    if (window_) {
        BringToFront();
        return true;
    }

    dpi_ = GetDpiForSystem();
    normalFont_.reset(CreateFontW(
        -MulDiv(10, static_cast<int>(dpi_), 72), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"));
    labelFont_.reset(CreateFontW(
        -MulDiv(10, static_cast<int>(dpi_), 72), 0, 0, 0, FW_SEMIBOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"));

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = kSettingsClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    RegisterClassExW(&windowClass);

    RECT bounds{0, 0, Scale(620), Scale(390)};
    AdjustWindowRectExForDpi(&bounds, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             FALSE, 0, dpi_);
    RECT working{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &working, 0);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    window_ = CreateWindowExW(
        0, kSettingsClassName, L"YanSnap 设置",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        working.left + (working.right - working.left - width) / 2,
        working.top + (working.bottom - working.top - height) / 2,
        width, height, owner_, nullptr, instance_, this);
    if (!window_) {
        return false;
    }

    largeIcon_.reset(CreateYanSnapIcon(32));
    smallIcon_.reset(CreateYanSnapIcon(16));
    SendMessageW(window_, WM_SETICON, ICON_BIG,
                 reinterpret_cast<LPARAM>(largeIcon_.get()));
    SendMessageW(window_, WM_SETICON, ICON_SMALL,
                 reinterpret_cast<LPARAM>(smallIcon_.get()));
    ShowWindow(window_, SW_SHOW);
    UpdateWindow(window_);
    return true;
}

void SettingsWindow::BringToFront() const {
    if (window_) {
        ShowWindow(window_, SW_RESTORE);
        SetForegroundWindow(window_);
    }
}

LRESULT CALLBACK SettingsWindow::WindowProc(HWND window, UINT message,
                                             WPARAM wParam, LPARAM lParam) {
    auto* settingsWindow =
        reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        settingsWindow = static_cast<SettingsWindow*>(create->lpCreateParams);
        settingsWindow->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(settingsWindow));
    }
    return settingsWindow ? settingsWindow->HandleMessage(message, wParam, lParam)
                          : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT SettingsWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateControls();
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
        SetTextColor(dc, kTextColor);
        SetBkColor(dc, kCardColor);
        return reinterpret_cast<LRESULT>(cardBrush_.get());
    }
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        const HWND control = reinterpret_cast<HWND>(lParam);
        SetTextColor(dc, kMutedTextColor);
        if (control == notificationCheck_ || control == cursorCheck_ ||
            control == startupCheck_) {
            SetBkColor(dc, kCardColor);
            return reinterpret_cast<LRESULT>(cardBrush_.get());
        }
        SetBkColor(dc, kPageColor);
        return reinterpret_cast<LRESULT>(pageBrush_.get());
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, kTextColor);
        SetBkColor(dc, kCardColor);
        return reinterpret_cast<LRESULT>(cardBrush_.get());
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kBrowseButton) {
            BrowseDirectory();
        } else if (LOWORD(wParam) == kSaveButton) {
            SaveAndClose();
        } else if (LOWORD(wParam) == kCancelButton) {
            DestroyWindow(window_);
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(window_);
        return 0;
    case WM_DESTROY:
        window_ = nullptr;
        return 0;
    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
}

void SettingsWindow::CreateControls() {
    const auto create = [this](const wchar_t* className, const wchar_t* text,
                               DWORD style, int x, int y, int width, int height,
                               int id, HFONT font) {
        HWND control = CreateWindowExW(
            0, className, text, WS_CHILD | WS_VISIBLE | style,
            Scale(x), Scale(y), Scale(width), Scale(height), window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return control;
    };

    create(L"STATIC", L"截图快捷键",
           SS_CENTERIMAGE, 48, 34, 130, 36, 0, labelFont_.get());
    hotkeyEdit_ = create(HOTKEY_CLASS, L"", WS_BORDER | WS_TABSTOP,
                         200, 34, 360, 36, kHotkeyEdit, normalFont_.get());
    SendMessageW(hotkeyEdit_, HKM_SETHOTKEY,
                 ToHotkeyControlValue(settings_.hotkeyModifiers,
                                      settings_.hotkeyKey),
                 0);

    create(L"STATIC", L"完成动作",
           SS_CENTERIMAGE, 48, 92, 130, 36, 0, labelFont_.get());
    actionCombo_ = create(WC_COMBOBOXW, L"",
                          CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                          200, 92, 360, 180, kActionCombo, normalFont_.get());
    SendMessageW(actionCombo_, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"仅复制"));
    SendMessageW(actionCombo_, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"复制并自动保存"));
    SendMessageW(actionCombo_, CB_SETCURSEL,
                 settings_.defaultAction == DefaultAction::CopyAndAutoSave ? 1 : 0,
                 0);
    SendMessageW(actionCombo_, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), Scale(27));

    create(L"STATIC", L"保存位置",
           SS_CENTERIMAGE, 48, 150, 130, 36, 0, labelFont_.get());
    directoryEdit_ = create(L"EDIT", settings_.saveDirectory.c_str(),
                            WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
                            200, 150, 280, 36, kDirectoryEdit,
                            normalFont_.get());
    create(L"BUTTON", L"选择", BS_OWNERDRAW | WS_TABSTOP,
           490, 150, 70, 36, kBrowseButton, labelFont_.get());

    notificationCheck_ = create(
        L"BUTTON", L"成功后通知",
        BS_AUTOCHECKBOX | WS_TABSTOP, 48, 214, 220, 28,
        kNotificationCheck, normalFont_.get());
    SendMessageW(notificationCheck_, BM_SETCHECK,
                 settings_.showNotifications ? BST_CHECKED : BST_UNCHECKED, 0);
    cursorCheck_ = create(
        L"BUTTON", L"包含鼠标指针",
        BS_AUTOCHECKBOX | WS_TABSTOP, 320, 214, 220, 28,
        kCursorCheck, normalFont_.get());
    SendMessageW(cursorCheck_, BM_SETCHECK,
                 settings_.includeCursor ? BST_CHECKED : BST_UNCHECKED, 0);

    startupCheck_ = create(
        L"BUTTON", L"开机自动启动",
        BS_AUTOCHECKBOX | WS_TABSTOP, 48, 258, 220, 28,
        kStartupCheck, normalFont_.get());
    SendMessageW(startupCheck_, BM_SETCHECK,
                 settings_.startWithWindows ? BST_CHECKED : BST_UNCHECKED, 0);

    create(L"BUTTON", L"保存", BS_OWNERDRAW | WS_TABSTOP,
           396, 330, 100, 38, kSaveButton, labelFont_.get());
    create(L"BUTTON", L"取消", BS_OWNERDRAW | WS_TABSTOP,
           508, 330, 88, 38, kCancelButton, labelFont_.get());
}

void SettingsWindow::Paint() {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window_, &paint);
    RECT client{};
    GetClientRect(window_, &client);
    FillRect(dc, &client, pageBrush_.get());

    UniqueBrush shadow(CreateSolidBrush(RGB(226, 230, 238)));
    UniquePen shadowPen(CreatePen(PS_SOLID, 1, RGB(226, 230, 238)));
    SelectObjectGuard selectedShadow(dc, shadow.get());
    SelectObjectGuard selectedShadowPen(dc, shadowPen.get());
    RoundRect(dc, Scale(25), Scale(23), Scale(597), Scale(307),
              Scale(14), Scale(14));

    UniquePen border(CreatePen(PS_SOLID, 1, kBorderColor));
    SelectObjectGuard selectedCard(dc, cardBrush_.get());
    SelectObjectGuard selectedBorder(dc, border.get());
    RoundRect(dc, Scale(24), Scale(20), Scale(596), Scale(304),
              Scale(14), Scale(14));

    UniquePen separator(CreatePen(PS_SOLID, 1, RGB(237, 240, 245)));
    SelectObjectGuard selectedSeparator(dc, separator.get());
    for (int y : {81, 139, 197, 247}) {
        MoveToEx(dc, Scale(48), Scale(y), nullptr);
        LineTo(dc, Scale(572), Scale(y));
    }

    EndPaint(window_, &paint);
}

void SettingsWindow::DrawButton(const DRAWITEMSTRUCT& draw) const {
    const bool pressed = (draw.itemState & ODS_SELECTED) != 0;
    const bool primary = draw.CtlID == kSaveButton;
    const bool browse = draw.CtlID == kBrowseButton;

    COLORREF background = kCardColor;
    COLORREF border = kBorderColor;
    COLORREF text = kTextColor;
    if (primary) {
        background = pressed ? RGB(22, 91, 190) : kAccentColor;
        border = background;
        text = RGB(255, 255, 255);
    } else if (browse) {
        background = pressed ? RGB(220, 233, 252) : RGB(239, 245, 255);
        border = RGB(183, 207, 241);
        text = RGB(25, 101, 207);
    } else if (pressed) {
        background = RGB(235, 238, 244);
    }

    UniqueBrush buttonBrush(CreateSolidBrush(background));
    UniquePen buttonPen(CreatePen(PS_SOLID, 1, border));
    SelectObjectGuard selectedBrush(draw.hDC, buttonBrush.get());
    SelectObjectGuard selectedPen(draw.hDC, buttonPen.get());
    RoundRect(draw.hDC, draw.rcItem.left, draw.rcItem.top,
              draw.rcItem.right, draw.rcItem.bottom, Scale(9), Scale(9));

    wchar_t textBuffer[64]{};
    GetWindowTextW(draw.hwndItem, textBuffer, ARRAYSIZE(textBuffer));
    SelectObjectGuard selectedFont(draw.hDC, labelFont_.get());
    SetBkMode(draw.hDC, TRANSPARENT);
    SetTextColor(draw.hDC, text);
    RECT textRect = draw.rcItem;
    DrawTextW(draw.hDC, textBuffer, -1, &textRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void SettingsWindow::BrowseDirectory() {
    BROWSEINFOW browse{};
    browse.hwndOwner = window_;
    browse.lpszTitle = L"选择默认截图保存目录";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&browse);
    if (!item) {
        return;
    }
    wchar_t path[32768]{};
    if (SHGetPathFromIDListW(item, path)) {
        SetWindowTextW(directoryEdit_, path);
    }
    CoTaskMemFree(item);
}

void SettingsWindow::SaveAndClose() {
    const WORD hotkey =
        static_cast<WORD>(SendMessageW(hotkeyEdit_, HKM_GETHOTKEY, 0, 0));
    const UINT hotkeyKey = LOBYTE(hotkey);
    const UINT hotkeyModifiers = FromHotkeyControlFlags(HIBYTE(hotkey));
    if (hotkeyKey == 0) {
        MessageBoxW(window_, L"请按下快捷键。", L"YanSnap",
                    MB_OK | MB_ICONWARNING);
        SetFocus(hotkeyEdit_);
        return;
    }

    Settings updated = settings_;
    updated.hotkeyModifiers = hotkeyModifiers;
    updated.hotkeyKey = hotkeyKey;
    updated.defaultAction =
        SendMessageW(actionCombo_, CB_GETCURSEL, 0, 0) == 1
            ? DefaultAction::CopyAndAutoSave
            : DefaultAction::CopyOnly;
    updated.saveDirectory = ControlText(directoryEdit_);
    if (updated.saveDirectory.empty()) {
        updated.saveDirectory = Settings::Defaults().saveDirectory;
    }
    updated.showNotifications =
        SendMessageW(notificationCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.includeCursor =
        SendMessageW(cursorCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    updated.startWithWindows =
        SendMessageW(startupCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (callback_ && !callback_(updated)) {
        return;
    }
    DestroyWindow(window_);
}

}  // namespace snaplite
