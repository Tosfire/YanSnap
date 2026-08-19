#pragma once

#include <windows.h>

#include "annotation/Annotation.h"
#include "common/Geometry.h"

namespace snaplite {

enum class ToolbarAction {
    None,
    Rectangle,
    Arrow,
    Pen,
    Text,
    Mosaic,
    Undo,
    Redo,
    Ocr,
    Pin,
    Save,
    Copy,
    Cancel,
};

class Toolbar {
public:
    void Update(RectI selectionClient, RectI desktopClient, UINT dpi);
    void Draw(HDC dc, AnnotationTool selectedTool, bool canUndo, bool canRedo,
              ToolbarAction hoveredAction, bool showTooltip, RectI desktopClient) const;
    [[nodiscard]] ToolbarAction HitTest(POINT clientPoint) const;
    [[nodiscard]] RectI Bounds() const noexcept { return bounds_; }

private:
    int buttonSize_{38};
    RectI bounds_{};
};

}  // namespace snaplite
