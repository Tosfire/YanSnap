#include <cassert>
#include <filesystem>
#include <iostream>
#include <string_view>

#include "settings/Settings.h"

int wmain(int argumentCount, wchar_t** arguments) {
    if (argumentCount == 2 &&
        std::wstring_view(arguments[1]) == L"--print-config-path") {
        std::wcout << snaplite::Settings::ConfigFilePath();
        return 0;
    }
    if (argumentCount == 2 &&
        std::wstring_view(arguments[1]) == L"--verify-portable-path") {
        std::wstring executablePath(32768, L'\0');
        const DWORD length = GetModuleFileNameW(
            nullptr, executablePath.data(),
            static_cast<DWORD>(executablePath.size()));
        executablePath.resize(length);
        const std::wstring expected =
            (std::filesystem::path(executablePath).parent_path() /
             L"config.ini")
                .wstring();
        return snaplite::Settings::ConfigFilePath() == expected ? 0 : 1;
    }

    const snaplite::Settings defaults = snaplite::Settings::Defaults();
    assert(defaults.hotkeyModifiers == 0);
    assert(defaults.hotkeyKey == VK_F1);
    assert(!defaults.startWithWindows);

    const auto defaultHotkey = snaplite::Settings::ParseHotkey(L"F1");
    assert(defaultHotkey);
    assert(defaultHotkey->first == 0);
    assert(defaultHotkey->second == VK_F1);
    assert(snaplite::Settings::FormatHotkey(defaultHotkey->first, defaultHotkey->second) ==
           L"F1");

    const auto functionKey = snaplite::Settings::ParseHotkey(L"Alt+F12");
    assert(functionKey && functionKey->second == VK_F12);
    const auto singleLetter = snaplite::Settings::ParseHotkey(L"A");
    assert(singleLetter && singleLetter->first == 0 && singleLetter->second == 'A');
    assert(snaplite::Settings::FormatHotkey(0, 'A') == L"A");
    assert(!snaplite::Settings::ParseHotkey(L"Ctrl+Mouse1"));
    assert(!snaplite::Settings::ParseHotkey(L"Ctrl+A+B"));
    assert(!snaplite::Settings::ParseHotkey(L"Alt+F12x"));
    return 0;
}
