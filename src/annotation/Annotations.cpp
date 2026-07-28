#include "annotation/Annotation.h"

#include <algorithm>
#include <cmath>

#include "common/Win32.h"

namespace snaplite {

namespace {

POINT OffsetPoint(POINT point, const RenderContext& context) {
    point.x += context.offsetX;
    point.y += context.offsetY;
    return point;
}

double DistanceToLine(POINT point, POINT start, POINT end) {
    const double dx = static_cast<double>(end.x - start.x);
    const double dy = static_cast<double>(end.y - start.y);
    if (dx == 0.0 && dy == 0.0) {
        return std::hypot(static_cast<double>(point.x - start.x),
                          static_cast<double>(point.y - start.y));
    }
    const double projection = std::clamp(
        ((point.x - start.x) * dx + (point.y - start.y) * dy) / (dx * dx + dy * dy),
        0.0, 1.0);
    const double closestX = start.x + projection * dx;
    const double closestY = start.y + projection * dy;
    return std::hypot(point.x - closestX, point.y - closestY);
}

void DrawOutlinedText(HDC dc, RECT rect, const std::wstring& text, COLORREF color) {
    UniqueFont font(CreateFontW(-22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"));
    SelectObjectGuard selectedFont(dc, font.get());
    SetBkMode(dc, TRANSPARENT);
    const RECT original = rect;
    SetTextColor(dc, RGB(32, 32, 32));
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            if (x == 0 && y == 0) {
                continue;
            }
            rect = original;
            OffsetRect(&rect, x, y);
            DrawTextW(dc, text.c_str(), -1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);
        }
    }
    rect = original;
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), -1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);
}

}  // namespace

RectangleAnnotation::RectangleAnnotation(POINT start, POINT end, COLORREF color, int width)
    : start_(start), end_(end), color_(color), width_(width) {}

void RectangleAnnotation::Draw(RenderContext& context) const {
    const POINT start = OffsetPoint(start_, context);
    const POINT end = OffsetPoint(end_, context);
    UniquePen pen(CreatePen(PS_SOLID, width_, color_));
    SelectObjectGuard selectedPen(context.dc, pen.get());
    HGDIOBJ oldBrush = SelectObject(context.dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(context.dc, start.x, start.y, end.x, end.y);
    SelectObject(context.dc, oldBrush);
}

RectI RectangleAnnotation::Bounds() const {
    return RectI::FromPoints(start_, end_);
}

bool RectangleAnnotation::HitTest(POINT point) const {
    RectI bounds = Bounds();
    const int tolerance = width_ + 4;
    RectI outer{bounds.left - tolerance, bounds.top - tolerance,
                bounds.right + tolerance, bounds.bottom + tolerance};
    RectI inner{bounds.left + tolerance, bounds.top + tolerance,
                bounds.right - tolerance, bounds.bottom - tolerance};
    return outer.Contains(point) && (inner.Empty() || !inner.Contains(point));
}

ArrowAnnotation::ArrowAnnotation(POINT start, POINT end, COLORREF color, int width)
    : start_(start), end_(end), color_(color), width_(width) {}

void ArrowAnnotation::Draw(RenderContext& context) const {
    const POINT start = OffsetPoint(start_, context);
    const POINT end = OffsetPoint(end_, context);
    UniquePen pen(CreatePen(PS_SOLID, width_, color_));
    SelectObjectGuard selectedPen(context.dc, pen.get());
    MoveToEx(context.dc, start.x, start.y, nullptr);
    LineTo(context.dc, end.x, end.y);

    const double angle = std::atan2(static_cast<double>(end.y - start.y),
                                    static_cast<double>(end.x - start.x));
    const double length = std::max(10.0, static_cast<double>(width_ * 5));
    const double spread = 0.55;
    POINT head[3] = {
        end,
        {static_cast<LONG>(std::lround(end.x - length * std::cos(angle - spread))),
         static_cast<LONG>(std::lround(end.y - length * std::sin(angle - spread)))},
        {static_cast<LONG>(std::lround(end.x - length * std::cos(angle + spread))),
         static_cast<LONG>(std::lround(end.y - length * std::sin(angle + spread)))},
    };
    UniqueBrush brush(CreateSolidBrush(color_));
    SelectObjectGuard selectedBrush(context.dc, brush.get());
    Polygon(context.dc, head, 3);
}

RectI ArrowAnnotation::Bounds() const {
    RectI bounds = RectI::FromPoints(start_, end_);
    bounds.left -= 12;
    bounds.top -= 12;
    bounds.right += 12;
    bounds.bottom += 12;
    return bounds;
}

bool ArrowAnnotation::HitTest(POINT point) const {
    return DistanceToLine(point, start_, end_) <= width_ + 5;
}

PenAnnotation::PenAnnotation(POINT start, COLORREF color, int width)
    : points_{start}, color_(color), width_(width) {}

void PenAnnotation::AddPoint(POINT point) {
    if (points_.empty() ||
        std::abs(point.x - points_.back().x) + std::abs(point.y - points_.back().y) >= 2) {
        points_.push_back(point);
    }
}

void PenAnnotation::Draw(RenderContext& context) const {
    if (points_.empty()) {
        return;
    }
    std::vector<POINT> points = points_;
    for (POINT& point : points) {
        point = OffsetPoint(point, context);
    }
    UniquePen pen(CreatePen(PS_SOLID, width_, color_));
    SelectObjectGuard selectedPen(context.dc, pen.get());
    if (points.size() == 1) {
        Ellipse(context.dc, points[0].x - width_, points[0].y - width_,
                points[0].x + width_, points[0].y + width_);
    } else {
        Polyline(context.dc, points.data(), static_cast<int>(points.size()));
    }
}

RectI PenAnnotation::Bounds() const {
    if (points_.empty()) {
        return {};
    }
    RectI bounds{points_[0].x, points_[0].y, points_[0].x + 1, points_[0].y + 1};
    for (POINT point : points_) {
        bounds.left = std::min(bounds.left, static_cast<int>(point.x));
        bounds.top = std::min(bounds.top, static_cast<int>(point.y));
        bounds.right = std::max(bounds.right, static_cast<int>(point.x + 1));
        bounds.bottom = std::max(bounds.bottom, static_cast<int>(point.y + 1));
    }
    return bounds;
}

bool PenAnnotation::HitTest(POINT point) const {
    for (std::size_t index = 1; index < points_.size(); ++index) {
        if (DistanceToLine(point, points_[index - 1], points_[index]) <= width_ + 5) {
            return true;
        }
    }
    return !points_.empty() && DistanceToLine(point, points_.front(), points_.front()) <= width_ + 5;
}

TextAnnotation::TextAnnotation(POINT position, std::wstring text, COLORREF color)
    : position_(position), text_(std::move(text)), color_(color) {}

void TextAnnotation::Draw(RenderContext& context) const {
    const POINT position = OffsetPoint(position_, context);
    RECT textRect{position.x, position.y, position.x + 320, position.y + 180};
    DrawOutlinedText(context.dc, textRect, text_, color_);
}

RectI TextAnnotation::Bounds() const {
    const int lines = 1 + static_cast<int>(std::count(text_.begin(), text_.end(), L'\n'));
    const int width = std::clamp(static_cast<int>(text_.size()) * 14, 24, 320);
    return RectI{position_.x, position_.y, position_.x + width, position_.y + lines * 28};
}

bool TextAnnotation::HitTest(POINT point) const {
    return Bounds().Contains(point);
}

MosaicAnnotation::MosaicAnnotation(POINT start, POINT end, int blockSize)
    : start_(start), end_(end), blockSize_(blockSize) {}

void MosaicAnnotation::Draw(RenderContext& context) const {
    RectI rect = Bounds();
    rect.left += context.offsetX;
    rect.right += context.offsetX;
    rect.top += context.offsetY;
    rect.bottom += context.offsetY;
    rect = Intersect(rect, RectI{0, 0, context.width, context.height});

    if (!context.preview && context.pixels && !rect.Empty()) {
        for (int blockY = rect.top; blockY < rect.bottom; blockY += blockSize_) {
            for (int blockX = rect.left; blockX < rect.right; blockX += blockSize_) {
                const int sampleX = std::min(blockX + blockSize_ / 2, rect.right - 1);
                const int sampleY = std::min(blockY + blockSize_ / 2, rect.bottom - 1);
                const auto* sample = context.pixels +
                    static_cast<std::size_t>(sampleY) * context.stride +
                    static_cast<std::size_t>(sampleX) * 4;
                const int endY = std::min(blockY + blockSize_, rect.bottom);
                const int endX = std::min(blockX + blockSize_, rect.right);
                for (int y = blockY; y < endY; ++y) {
                    for (int x = blockX; x < endX; ++x) {
                        auto* pixel = context.pixels + static_cast<std::size_t>(y) * context.stride +
                                      static_cast<std::size_t>(x) * 4;
                        pixel[0] = sample[0];
                        pixel[1] = sample[1];
                        pixel[2] = sample[2];
                        pixel[3] = 255;
                    }
                }
            }
        }
        return;
    }

    UniquePen pen(CreatePen(PS_DOT, 1, RGB(220, 220, 220)));
    SelectObjectGuard selectedPen(context.dc, pen.get());
    HGDIOBJ oldBrush = SelectObject(context.dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(context.dc, rect.left, rect.top, rect.right, rect.bottom);
    for (int x = rect.left + blockSize_; x < rect.right; x += blockSize_) {
        MoveToEx(context.dc, x, rect.top, nullptr);
        LineTo(context.dc, x, rect.bottom);
    }
    for (int y = rect.top + blockSize_; y < rect.bottom; y += blockSize_) {
        MoveToEx(context.dc, rect.left, y, nullptr);
        LineTo(context.dc, rect.right, y);
    }
    SelectObject(context.dc, oldBrush);
}

RectI MosaicAnnotation::Bounds() const {
    return RectI::FromPoints(start_, end_);
}

bool MosaicAnnotation::HitTest(POINT point) const {
    return Bounds().Contains(point);
}

}  // namespace snaplite

