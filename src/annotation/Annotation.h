#pragma once

#include <windows.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "common/Geometry.h"

namespace snaplite {

enum class AnnotationTool {
    None,
    Rectangle,
    Arrow,
    Pen,
    Text,
    Mosaic,
};

struct RenderContext {
    HDC dc{};
    std::uint8_t* pixels{};
    int width{};
    int height{};
    int stride{};
    int offsetX{};
    int offsetY{};
    bool preview{};
};

class Annotation {
public:
    virtual ~Annotation() = default;
    virtual void Draw(RenderContext& context) const = 0;
    [[nodiscard]] virtual RectI Bounds() const = 0;
    [[nodiscard]] virtual bool HitTest(POINT point) const = 0;
};

using AnnotationPtr = std::shared_ptr<Annotation>;

class RectangleAnnotation final : public Annotation {
public:
    RectangleAnnotation(POINT start, POINT end, COLORREF color = RGB(236, 55, 61), int width = 3);
    void Draw(RenderContext& context) const override;
    [[nodiscard]] RectI Bounds() const override;
    [[nodiscard]] bool HitTest(POINT point) const override;

private:
    POINT start_{};
    POINT end_{};
    COLORREF color_{};
    int width_{};
};

class ArrowAnnotation final : public Annotation {
public:
    ArrowAnnotation(POINT start, POINT end, COLORREF color = RGB(236, 55, 61), int width = 3);
    void Draw(RenderContext& context) const override;
    [[nodiscard]] RectI Bounds() const override;
    [[nodiscard]] bool HitTest(POINT point) const override;

private:
    POINT start_{};
    POINT end_{};
    COLORREF color_{};
    int width_{};
};

class PenAnnotation final : public Annotation {
public:
    explicit PenAnnotation(POINT start, COLORREF color = RGB(236, 55, 61), int width = 3);
    void AddPoint(POINT point);
    void Draw(RenderContext& context) const override;
    [[nodiscard]] RectI Bounds() const override;
    [[nodiscard]] bool HitTest(POINT point) const override;

private:
    std::vector<POINT> points_;
    COLORREF color_{};
    int width_{};
};

class TextAnnotation final : public Annotation {
public:
    TextAnnotation(POINT position, std::wstring text, COLORREF color = RGB(236, 55, 61));
    void Draw(RenderContext& context) const override;
    [[nodiscard]] RectI Bounds() const override;
    [[nodiscard]] bool HitTest(POINT point) const override;

private:
    POINT position_{};
    std::wstring text_;
    COLORREF color_{};
};

class MosaicAnnotation final : public Annotation {
public:
    MosaicAnnotation(POINT start, POINT end, int blockSize = 12);
    void Draw(RenderContext& context) const override;
    [[nodiscard]] RectI Bounds() const override;
    [[nodiscard]] bool HitTest(POINT point) const override;

private:
    POINT start_{};
    POINT end_{};
    int blockSize_{};
};

}  // namespace snaplite

