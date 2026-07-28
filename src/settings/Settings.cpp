#include "settings/Settings.h"

#include <shlobj.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <sstream>
#include <vector>

#include "export/PngEncoder.h"

namespace snaplite {

namespace {

constexpr wchar_t kSettingsSection[] = L"YanSnap";
constexpr wchar_t kLegacySettingsSection[] = L"SnapLite";

std::wstring ReadIniString(const std::wstring& path, const wchar_t* section,
                           const wchar_t* key,
                           const wchar_t* fallback = L"") {
    std::vector<wchar_t> buffer(32768, L'\0');
    GetPrivateProfileStringW(section, key, fallback, buffer.data(),
                             static_cast<DWORD>(buffer.size()), path.c_str());
    return buffer.data();
}

void BackupCorruptFile(const std::wstring& path) {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t suffix[64]{};
    wsprintfW(suffix, L".corrupt_%04u%02u%02u_%02u%02u%02u",
              time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    std::wstring backup = path + suffix;
    MoveFileExW(path.c_str(), backup.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

bool WriteIniValue(const std::wstring& path, const wchar_t* key, const std::wstring& value) {
    return WritePrivateProfileStringW(kSettingsSection, key, value.c_str(), path.c_str()) != FALSE;
}

std::optional<std::wstring> PortableSettingsPath() {
    std::wstring executablePath(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    if (length == 0 || length >= executablePath.size()) {
        return std::nullopt;
    }
    executablePath.resize(length);
    const std::filesystem::path directory =
        std::filesystem::path(executablePath).parent_path();
    if (GetFileAttributesW((directory / L"portable.flag").c_str()) ==
        INVALID_FILE_ATTRIBUTES) {
        return std::nullopt;
    }
    return (directory / L"config.ini").wstring();
}

std::wstring SettingsPathFor(const wchar_t* productDirectory) {
    PWSTR localAppData = nullptr;
    std::filesystem::path path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE,
                                       nullptr, &localAppData))) {
        path = std::filesystem::path(localAppData) / productDirectory / L"settings.ini";
        CoTaskMemFree(localAppData);
    } else {
        wchar_t fallback[32768]{};
        GetEnvironmentVariableW(L"LOCALAPPDATA", fallback, ARRAYSIZE(fallback));
        path = std::filesystem::path(fallback) / productDirectory / L"settings.ini";
    }
    return path.wstring();
}

}  // namespace

Settings Settings::Defaults() {
    Settings settings;
    settings.saveDirectory = PngEncoder::DefaultScreenshotsDirectory();
    return settings;
}

Settings Settings::Load() {
    Settings settings = Defaults();
    const bool portableMode = PortableSettingsPath().has_value();
    std::wstring path = ConfigFilePath();
    const wchar_t* section = kSettingsSection;
    bool migrateLegacySettings = false;
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (portableMode) {
            return settings;
        }
        const std::wstring legacyPath = SettingsPathFor(L"SnapLite");
        if (GetFileAttributesW(legacyPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            return settings;
        }
        path = legacyPath;
        section = kLegacySettingsSection;
        migrateLegacySettings = true;
    }
    if (ReadIniString(path, section, L"Version") != L"1") {
        BackupCorruptFile(path);
        return settings;
    }

    const UINT modifiers = GetPrivateProfileIntW(section, L"HotkeyModifiers",
                                                  settings.hotkeyModifiers, path.c_str());
    const UINT key = GetPrivateProfileIntW(section, L"HotkeyKey",
                                           settings.hotkeyKey, path.c_str());
    if (!FormatHotkey(modifiers, key).empty()) {
        settings.hotkeyModifiers = modifiers;
        settings.hotkeyKey = key;
    }
    const int action = GetPrivateProfileIntW(section, L"DefaultAction", 0, path.c_str());
    settings.defaultAction = action == 1 ? DefaultAction::CopyAndAutoSave : DefaultAction::CopyOnly;
    const std::wstring directory = ReadIniString(path, section, L"SaveDirectory");
    if (!directory.empty()) {
        settings.saveDirectory = directory;
    }
    settings.showNotifications =
        GetPrivateProfileIntW(section, L"ShowNotifications", 1, path.c_str()) != 0;
    settings.includeCursor =
        GetPrivateProfileIntW(section, L"IncludeCursor", 0, path.c_str()) != 0;
    settings.startWithWindows =
        GetPrivateProfileIntW(section, L"StartWithWindows", 0, path.c_str()) != 0;
    if (migrateLegacySettings) {
        settings.Save();
    }
    return settings;
}

bool Settings::Save(std::wstring* error) const {
    const std::filesystem::path destination(ConfigFilePath());
    std::error_code directoryError;
    std::filesystem::create_directories(destination.parent_path(), directoryError);
    if (directoryError) {
        if (error) {
            *error = L"无法创建设置目录。";
        }
        return false;
    }
    const std::wstring temporary = destination.wstring() + L".tmp";
    DeleteFileW(temporary.c_str());
    const bool written =
        WriteIniValue(temporary, L"Version", L"1") &&
        WriteIniValue(temporary, L"HotkeyModifiers", std::to_wstring(hotkeyModifiers)) &&
        WriteIniValue(temporary, L"HotkeyKey", std::to_wstring(hotkeyKey)) &&
        WriteIniValue(temporary, L"DefaultAction",
                      std::to_wstring(static_cast<int>(defaultAction))) &&
        WriteIniValue(temporary, L"SaveDirectory", saveDirectory) &&
        WriteIniValue(temporary, L"ShowNotifications", showNotifications ? L"1" : L"0") &&
        WriteIniValue(temporary, L"IncludeCursor", includeCursor ? L"1" : L"0") &&
        WriteIniValue(temporary, L"StartWithWindows",
                      startWithWindows ? L"1" : L"0");
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, temporary.c_str());
    if (!written || !MoveFileExW(temporary.c_str(), destination.c_str(),
                                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        if (error) {
            *error = L"设置写入失败。";
        }
        return false;
    }
    return true;
}

std::wstring Settings::ConfigFilePath() {
    if (const auto portablePath = PortableSettingsPath()) {
        return *portablePath;
    }
    return SettingsPathFor(L"YanSnap");
}

std::wstring Settings::FormatHotkey(UINT modifiers, UINT key) {
    std::wstring result;
    if (modifiers & MOD_CONTROL) {
        result += L"Ctrl+";
    }
    if (modifiers & MOD_SHIFT) {
        result += L"Shift+";
    }
    if (modifiers & MOD_ALT) {
        result += L"Alt+";
    }
    if (modifiers & MOD_WIN) {
        result += L"Win+";
    }
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) {
        result.push_back(static_cast<wchar_t>(key));
    } else if (key >= VK_F1 && key <= VK_F24) {
        result += L"F" + std::to_wstring(key - VK_F1 + 1);
    } else {
        return {};
    }
    return result;
}

std::optional<std::pair<UINT, UINT>> Settings::ParseHotkey(std::wstring value) {
    value.erase(std::remove_if(value.begin(), value.end(),
                               [](wchar_t character) { return std::iswspace(character) != 0; }),
                value.end());
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t character) { return std::towupper(character); });
    std::wstringstream stream(value);
    std::wstring part;
    UINT modifiers = 0;
    UINT key = 0;
    while (std::getline(stream, part, L'+')) {
        if (part == L"CTRL" || part == L"CONTROL") {
            modifiers |= MOD_CONTROL;
        } else if (part == L"SHIFT") {
            modifiers |= MOD_SHIFT;
        } else if (part == L"ALT") {
            modifiers |= MOD_ALT;
        } else if (part == L"WIN" || part == L"WINDOWS") {
            modifiers |= MOD_WIN;
        } else if (part.size() == 1 &&
                   ((part[0] >= L'A' && part[0] <= L'Z') ||
                    (part[0] >= L'0' && part[0] <= L'9'))) {
            if (key != 0) {
                return std::nullopt;
            }
            key = static_cast<UINT>(part[0]);
        } else if (part.size() >= 2 && part[0] == L'F') {
            if (key != 0 ||
                !std::all_of(part.begin() + 1, part.end(),
                             [](wchar_t character) { return std::iswdigit(character) != 0; })) {
                return std::nullopt;
            }
            const int functionKey = _wtoi(part.c_str() + 1);
            if (functionKey >= 1 && functionKey <= 24) {
                key = VK_F1 + static_cast<UINT>(functionKey - 1);
            } else {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    if (key == 0) {
        return std::nullopt;
    }
    return std::pair{modifiers, key};
}

}  // namespace snaplite
