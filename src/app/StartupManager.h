#pragma once

#include <string>

namespace snaplite {

bool ConfigureStartWithWindows(bool enabled, std::wstring* error = nullptr);

}  // namespace snaplite
