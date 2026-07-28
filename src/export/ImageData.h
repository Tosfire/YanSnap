#pragma once

#include <cstdint>
#include <vector>

namespace snaplite {

struct ImageData {
    int width{};
    int height{};
    int stride{};
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] bool Valid() const noexcept {
        return width > 0 && height > 0 &&
               pixels.size() == static_cast<std::size_t>(stride) * static_cast<std::size_t>(height);
    }
};

}  // namespace snaplite

