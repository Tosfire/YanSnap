#pragma once

#include <windows.h>

#include <algorithm>
#include <cstdlib>

namespace snaplite {

struct RectI {
    int left{};
    int top{};
    int right{};
    int bottom{};

    [[nodiscard]] int Width() const noexcept { return right - left; }
    [[nodiscard]] int Height() const noexcept { return bottom - top; }
    [[nodiscard]] bool Empty() const noexcept { return Width() <= 0 || Height() <= 0; }
    [[nodiscard]] bool Contains(POINT point) const noexcept {
        return point.x >= left && point.x < right && point.y >= top && point.y < bottom;
    }

    [[nodiscard]] RECT ToRect() const noexcept { return RECT{left, top, right, bottom}; }

    static RectI FromPoints(POINT first, POINT second) noexcept {
        return RectI{
            std::min(first.x, second.x),
            std::min(first.y, second.y),
            std::max(first.x, second.x),
            std::max(first.y, second.y),
        };
    }

    static RectI FromRect(const RECT& rect) noexcept {
        return RectI{rect.left, rect.top, rect.right, rect.bottom};
    }
};

inline bool operator==(const RectI& left, const RectI& right) noexcept {
    return left.left == right.left && left.top == right.top &&
           left.right == right.right && left.bottom == right.bottom;
}

inline RectI Intersect(RectI first, RectI second) noexcept {
    RectI result{
        std::max(first.left, second.left),
        std::max(first.top, second.top),
        std::min(first.right, second.right),
        std::min(first.bottom, second.bottom),
    };
    return result.Empty() ? RectI{} : result;
}

inline RectI ClampInside(RectI rect, RectI bounds) noexcept {
    const int width = std::min(rect.Width(), bounds.Width());
    const int height = std::min(rect.Height(), bounds.Height());
    rect.left = std::clamp(rect.left, bounds.left, bounds.right - width);
    rect.top = std::clamp(rect.top, bounds.top, bounds.bottom - height);
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;
    return rect;
}

}  // namespace snaplite

