#include "app/App.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>

#include <algorithm>
#include <filesystem>
#include <utility>

#include "capture/DesktopCapture.h"
#include "common/AppIcon.h"
#include "export/ClipboardExporter.h"
#include "export/PngEncoder.h"
#include "overlay/OverlayWindow.h"
#include "pin/PinWindow.h"
#include "resources/resource.h"
#include "app/StartupManager.h"

namespace snaplite {

namespace {
constexpr wchar_t kWindowClassName[] = L"YanSnap.MainWindow.v1";
constexpr UINT kShowBackgroundStatusMessage = WM_APP + 1;
}

App::App(HINSTANCE instance) : instance_(instance), settings_(Settings::Load()) {}
App::~App() = default;

int App::Run() {
    if (!singleInstance_.IsPrimary()) {
        singleInstance_.NotifyPrimary();
        return 0;
    }
    if (!Initialize()) {
        MessageBoxW(nullptr, L"YanSnap 初始化失败。", L"YanSnap", MB_OK | MB_ICONERROR);
        return 1;
    }

    PostMessageW(window_, kShowBackgroundStatusMessage, 0, 0);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

bool App::Initialize() {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_HOTKEY_CLASS};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    appIcon_.reset(CreateYanSnapIcon(32));
    windowClass.hIcon = appIcon_ ? appIcon_.get() : LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    window_ = CreateWindowExW(0, kWindowClassName, L"", WS_OVERLAPPED,
                              0, 0, 0, 0, nullptr, nullptr, instance_, this);
    if (!window_) {
        return false;
    }

    if (!tray_.Create(window_)) {
        return false;
    }
    tray_.SetHotkeyLabel(Settings::FormatHotkey(settings_.hotkeyModifiers,
                                                settings_.hotkeyKey));
    hotkey_.Register(window_, settings_.hotkeyModifiers | MOD_NOREPEAT,
                     settings_.hotkeyKey);
    pinHotkeyRegistered_ =
        RegisterHotKey(window_, 2, MOD_NOREPEAT, VK_F3) != FALSE;
    if (settings_.startWithWindows) {
        ConfigureStartWithWindows(true);
    }
    return true;
}

void App::ShowBackgroundStatus() {
    std::wstring message;
    if (hotkey_.IsRegistered()) {
        message = L"按 " +
                  Settings::FormatHotkey(settings_.hotkeyModifiers,
                                         settings_.hotkeyKey) +
                  L" 开始截图";
        if (pinHotkeyRegistered_) {
            message += L"，按 F3 贴图";
        }
        message += L"；右键托盘图标可打开菜单。";
    } else {
        message = L"截图快捷键已被其他程序占用，请右键托盘图标打开菜单或设置新快捷键。";
    }
    tray_.Notify(L"YanSnap 已在后台运行", message.c_str());
}

void App::BeginCapture() {
    if (overlay_ && overlay_->Active()) {
        return;
    }
    std::vector<PinWindow*> visiblePins;
    for (const auto& pin : pins_) {
        if (pin->IsVisible()) {
            visiblePins.push_back(pin.get());
            pin->SetVisible(false);
        }
    }
    DwmFlush();
    DesktopImage image = DesktopCapture::Capture(settings_.includeCursor);
    for (PinWindow* pin : visiblePins) {
        pin->SetVisible(true);
    }
    if (!image.Valid()) {
        tray_.Notify(L"截图失败", L"无法捕获当前桌面。");
        return;
    }
    overlay_ = std::make_unique<OverlayWindow>(
        instance_, window_, &tray_, settings_,
        [this](ImageData output, POINT position) {
            CreatePin(std::move(output), position);
        });
    if (!overlay_->Start(std::move(image))) {
        overlay_.reset();
        tray_.Notify(L"截图失败", L"无法创建截图遮罩。");
    }
}

void App::HandleCommand(UINT command) {
    switch (command) {
    case ID_TRAY_CAPTURE:
        BeginCapture();
        break;
    case ID_TRAY_PIN_CLIPBOARD:
        PinClipboard();
        break;
    case ID_TRAY_TOGGLE_PINS:
        TogglePins();
        break;
    case ID_TRAY_CLOSE_PINS:
        ClosePins();
        break;
    case ID_TRAY_OPEN_FOLDER: {
        std::error_code directoryError;
        std::filesystem::create_directories(settings_.saveDirectory, directoryError);
        ShellExecuteW(nullptr, L"open", settings_.saveDirectory.c_str(),
                      nullptr, nullptr, SW_SHOWNORMAL);
        break;
    }
    case ID_TRAY_SETTINGS:
        ShowSettings();
        break;
    case ID_TRAY_ABOUT:
        MessageBoxW(window_, L"YanSnap 1.1\n轻量、纯本地的 Windows 截图与文字识别工具。", L"关于 YanSnap",
                    MB_OK | MB_ICONINFORMATION);
        break;
    case ID_TRAY_EXIT:
        DestroyWindow(window_);
        break;
    default:
        break;
    }
}

void App::CreatePin(ImageData image, POINT screenPosition) {
    auto pin = std::make_unique<PinWindow>(
        instance_, window_, std::move(image), screenPosition);
    if (!pin->Show()) {
        MessageBoxW(window_, L"无法创建贴图窗口。", L"YanSnap",
                    MB_OK | MB_ICONERROR);
        return;
    }
    pins_.push_back(std::move(pin));
}

void App::PinClipboard() {
    auto image = ClipboardExporter::Read(window_);
    if (!image) {
        tray_.Notify(L"无法贴图", L"剪贴板中没有可用的位图。");
        return;
    }
    POINT position{};
    GetCursorPos(&position);
    position.x += 16;
    position.y += 16;
    CreatePin(std::move(*image), position);
}

void App::TogglePins() {
    const bool anyVisible = std::any_of(
        pins_.begin(), pins_.end(),
        [](const auto& pin) { return pin->IsVisible(); });
    for (const auto& pin : pins_) {
        pin->SetVisible(!anyVisible);
    }
}

void App::ClosePins() {
    for (const auto& pin : pins_) {
        pin->Close();
    }
}

void App::ShowSettings() {
    if (settingsWindow_ && settingsWindow_->Active()) {
        settingsWindow_->BringToFront();
        return;
    }
    settingsWindow_ = std::make_unique<SettingsWindow>(
        instance_, window_, settings_,
        [this](const Settings& updated) { return ApplySettings(updated); });
    if (!settingsWindow_->Show()) {
        settingsWindow_.reset();
        MessageBoxW(window_, L"无法打开设置窗口。", L"YanSnap", MB_OK | MB_ICONERROR);
    }
}

bool App::ApplySettings(const Settings& settings) {
    const Settings previous = settings_;
    const bool hotkeyChanged = settings.hotkeyModifiers != previous.hotkeyModifiers ||
                               settings.hotkeyKey != previous.hotkeyKey;
    const bool startupChanged =
        settings.startWithWindows != previous.startWithWindows;
    if (hotkeyChanged &&
        !hotkey_.Register(window_, settings.hotkeyModifiers | MOD_NOREPEAT,
                          settings.hotkeyKey)) {
        hotkey_.Register(window_, previous.hotkeyModifiers | MOD_NOREPEAT,
                         previous.hotkeyKey);
        MessageBoxW(window_, L"这个快捷键已被其他程序占用，请换一个组合键。",
                    L"YanSnap", MB_OK | MB_ICONWARNING);
        return false;
    }

    std::wstring error;
    if (startupChanged &&
        !ConfigureStartWithWindows(settings.startWithWindows, &error)) {
        if (hotkeyChanged) {
            hotkey_.Register(window_, previous.hotkeyModifiers | MOD_NOREPEAT,
                             previous.hotkeyKey);
        }
        MessageBoxW(window_, error.c_str(), L"YanSnap", MB_OK | MB_ICONERROR);
        return false;
    }
    if (!settings.Save(&error)) {
        if (startupChanged) {
            ConfigureStartWithWindows(previous.startWithWindows);
        }
        if (hotkeyChanged) {
            hotkey_.Register(window_, previous.hotkeyModifiers | MOD_NOREPEAT,
                             previous.hotkeyKey);
        }
        MessageBoxW(window_, error.c_str(), L"YanSnap", MB_OK | MB_ICONERROR);
        return false;
    }
    settings_ = settings;
    tray_.SetHotkeyLabel(Settings::FormatHotkey(settings_.hotkeyModifiers,
                                                settings_.hotkeyKey));
    if (!hotkeyChanged &&
        !hotkey_.Register(window_, settings_.hotkeyModifiers | MOD_NOREPEAT,
                          settings_.hotkeyKey)) {
        tray_.Notify(L"快捷键不可用", L"全局快捷键已被其他程序占用。");
    } else if (settings_.showNotifications) {
        tray_.Notify(L"YanSnap", L"设置已保存。");
    }
    return true;
}

LRESULT CALLBACK App::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    return app ? app->HandleMessage(window, message, wParam, lParam)
               : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT App::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == singleInstance_.ActivationMessage()) {
        ShowBackgroundStatus();
        return 0;
    }
    switch (message) {
    case kShowBackgroundStatusMessage:
        ShowBackgroundStatus();
        return 0;
    case WM_HOTKEY:
        if (wParam == HotkeyManager::kCaptureHotkeyId) {
            BeginCapture();
        } else if (wParam == 2) {
            PinClipboard();
        }
        return 0;
    case WM_COMMAND:
        HandleCommand(LOWORD(wParam));
        return 0;
    case OverlayWindow::kSessionEndedMessage:
        overlay_.reset();
        return 0;
    case PinWindow::kClosedMessage: {
        auto* closed = reinterpret_cast<PinWindow*>(wParam);
        pins_.erase(std::remove_if(
                        pins_.begin(), pins_.end(),
                        [closed](const auto& pin) { return pin.get() == closed; }),
                    pins_.end());
        return 0;
    }
    case TrayIcon::kCallbackMessage:
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            BeginCapture();
        } else if (LOWORD(lParam) == WM_CONTEXTMENU || LOWORD(lParam) == WM_RBUTTONUP) {
            POINT point{};
            GetCursorPos(&point);
            tray_.ShowMenu(point);
        }
        return 0;
    case WM_DESTROY:
        settingsWindow_.reset();
        pins_.clear();
        if (overlay_) {
            overlay_->Cancel();
            overlay_.reset();
        }
        hotkey_.Unregister();
        if (pinHotkeyRegistered_) {
            UnregisterHotKey(window_, 2);
            pinHotkeyRegistered_ = false;
        }
        tray_.Remove();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

}  // namespace snaplite
