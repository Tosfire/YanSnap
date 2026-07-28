#include "app/StartupManager.h"

#include <windows.h>

#include <string>
#include <vector>

namespace snaplite {

namespace {

constexpr wchar_t kRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"YanSnap";

std::wstring CurrentExecutableCommand() {
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    return L"\"" + std::wstring(buffer.data(), length) + L"\" --startup";
}

void SetError(std::wstring* error, const wchar_t* message) {
    if (error) {
        *error = message;
    }
}

}  // namespace

bool ConfigureStartWithWindows(bool enabled, std::wstring* error) {
    HKEY runKey = nullptr;
    if (enabled) {
        const std::wstring command = CurrentExecutableCommand();
        if (command.empty()) {
            SetError(error, L"无法获取 YanSnap 程序路径，开机自启设置失败。");
            return false;
        }

        const LSTATUS openResult = RegCreateKeyExW(
            HKEY_CURRENT_USER, kRunKey, 0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE, nullptr, &runKey, nullptr);
        if (openResult != ERROR_SUCCESS) {
            SetError(error, L"无法写入 Windows 开机启动项。");
            return false;
        }

        const DWORD byteCount =
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
        const LSTATUS writeResult = RegSetValueExW(
            runKey, kValueName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()), byteCount);
        RegCloseKey(runKey);
        if (writeResult != ERROR_SUCCESS) {
            SetError(error, L"无法写入 Windows 开机启动项。");
            return false;
        }
        return true;
    }

    const LSTATUS openResult = RegOpenKeyExW(
        HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &runKey);
    if (openResult == ERROR_FILE_NOT_FOUND) {
        return true;
    }
    if (openResult != ERROR_SUCCESS) {
        SetError(error, L"无法打开 Windows 开机启动项。");
        return false;
    }

    const LSTATUS deleteResult = RegDeleteValueW(runKey, kValueName);
    RegCloseKey(runKey);
    if (deleteResult != ERROR_SUCCESS &&
        deleteResult != ERROR_FILE_NOT_FOUND) {
        SetError(error, L"无法移除 Windows 开机启动项。");
        return false;
    }
    return true;
}

}  // namespace snaplite
