#pragma once

#include <windows.h>

#include <optional>
#include <string>

namespace snaplite {

enum class DefaultAction {
    CopyOnly = 0,
    CopyAndAutoSave = 1,
};

struct Settings {
    UINT hotkeyModifiers{};
    UINT hotkeyKey{VK_F1};
    DefaultAction defaultAction{DefaultAction::CopyOnly};
    std::wstring saveDirectory;
    bool showNotifications{true};
    bool includeCursor{false};
    bool startWithWindows{false};

    static Settings Defaults();
    static Settings Load();
    bool Save(std::wstring* error = nullptr) const;
    static std::wstring ConfigFilePath();

    static std::wstring FormatHotkey(UINT modifiers, UINT key);
    static std::optional<std::pair<UINT, UINT>> ParseHotkey(std::wstring value);
};

}  // namespace snaplite
