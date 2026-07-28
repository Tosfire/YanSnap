#pragma once

#include <cstdint>
#include <vector>

#include "common/Geometry.h"

namespace snaplite {

struct DesktopImage {
    int originX{};
    int originY{};
    int width{};
    int height{};
    int stride{};
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] bool Valid() const noexcept {
        return width > 0 && height > 0 &&
               pixels.size() == static_cast<std::size_t>(stride) * static_cast<std::size_t>(height);
    }
    [[nodiscard]] RectI Bounds() const noexcept {
        return RectI{originX, originY, originX + width, originY + height};
    }
};

class DesktopCapture {
public:
    static DesktopImage Capture(bool includeCursor = false);
};

}  // namespace snaplite

