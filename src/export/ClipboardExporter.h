#pragma once

#include <windows.h>

#include <optional>
#include <string_view>

#include "export/ImageData.h"

namespace snaplite {

class ClipboardExporter {
public:
    static bool Copy(HWND owner, const ImageData& image);
    static bool CopyText(HWND owner, std::wstring_view text);
    static std::optional<ImageData> Read(HWND owner);
};

}  // namespace snaplite
