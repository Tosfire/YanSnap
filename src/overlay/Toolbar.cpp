#include "overlay/Toolbar.h"

#include <algorithm>
#include <array>

#include "common/Win32.h"

namespace snaplite {

namespace {

constexpr int kButtonCount = 12;
constexpr std::array<ToolbarAction, kButtonCount> kActions = {
    ToolbarAction::Rectangle, ToolbarAction::Arrow, ToolbarAction::Pen,
    ToolbarAction::Text, ToolbarAction::Mosaic, ToolbarAction::Undo,
    ToolbarAction::Redo, ToolbarAction::Ocr, ToolbarAction::Pin, ToolbarAction::Save,
    ToolbarAction::Copy, ToolbarAction::Cancel,
};

bool IsSelected(ToolbarAction action, AnnotationTool tool) {
    return (action == ToolbarAction::Rectangle && tool == AnnotationTool::Rectangle) ||
           (action == ToolbarAction::Arrow && tool == AnnotationTool::Arrow) ||
           (action == ToolbarAction::Pen && tool == AnnotationTool::Pen) ||
           (action == ToolbarAction::Text && tool == AnnotationTool::Text) ||
           (action == ToolbarAction::Mosaic && tool == AnnotationTool::Mosaic);
}

void DrawIcon(HDC dc, ToolbarAction action, RECT rect, bool enabled) {
    const COLORREF color = enabled ? RGB(245, 245, 245) : RGB(110, 110, 110);
    UniquePen pen(CreatePen(PS_SOLID, 2, color));
    SelectObjectGuard selectedPen(dc, pen.get());
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    const int cx = (rect.left + rect.right) / 2;
    const int cy = (rect.top + rect.bottom) / 2;
    const int radius = (rect.right - rect.left) / 4;
    switch (action) {
    case ToolbarAction::Rectangle: {
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, cx - radius, cy - radius + 1, cx + radius, cy + radius - 1);
        SelectObject(dc, oldBrush);
        break;
    }
    case ToolbarAction::Arrow:
        MoveToEx(dc, cx - radius, cy + radius / 2, nullptr);
        LineTo(dc, cx + radius, cy - radius / 2);
        LineTo(dc, cx + radius - 5, cy - radius / 2);
        MoveToEx(dc, cx + radius, cy - radius / 2, nullptr);
        LineTo(dc, cx + radius, cy - radius / 2 + 5);
        break;
    case ToolbarAction::Pen:
        MoveToEx(dc, cx - radius, cy + radius / 2, nullptr);
        LineTo(dc, cx - radius / 3, cy - radius / 2);
        LineTo(dc, cx + radius / 3, cy + radius / 3);
        LineTo(dc, cx + radius, cy - radius / 2);
        break;
    case ToolbarAction::Text:
        DrawTextW(dc, L"T", 1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break;
    case ToolbarAction::Mosaic:
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                RECT square{cx + x * 6 - 2, cy + y * 6 - 2, cx + x * 6 + 3, cy + y * 6 + 3};
                FillRect(dc, &square, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            }
        }
        break;
    case ToolbarAction::Undo:
        DrawTextW(dc, L"↶", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break;
    case ToolbarAction::Redo:
        DrawTextW(dc, L"↷", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break;
    case ToolbarAction::Ocr:
        DrawTextW(dc, L"OCR", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        break;
    case ToolbarAction::Pin: {
        MoveToEx(dc, cx - radius + 1, cy - radius + 1, nullptr);
        LineTo(dc, cx + radius - 1, cy - radius + 1);
        MoveToEx(dc, cx - radius + 3, cy - radius + 1, nullptr);
        LineTo(dc, cx - radius + 4, cy - 2);
        LineTo(dc, cx - radius, cy + 3);
        LineTo(dc, cx + radius, cy + 3);
        LineTo(dc, cx + radius - 4, cy - 2);
        LineTo(dc, cx + radius - 3, cy - radius + 1);
        MoveToEx(dc, cx, cy + 3, nullptr);
        LineTo(dc, cx, cy + radius);
        break;
    }
    case ToolbarAction::Save:
        Rectangle(dc, cx - radius, cy - radius, cx + radius, cy + radius);
        Rectangle(dc, cx - radius / 2, cy - radius, cx + radius / 2, cy - 2);
        break;
    case ToolbarAction::Copy:
        MoveToEx(dc, cx - radius, cy, nullptr);
        LineTo(dc, cx - 2, cy + radius);
        LineTo(dc, cx + radius, cy - radius);
        break;
    case ToolbarAction::Cancel:
        MoveToEx(dc, cx - radius, cy - radius, nullptr);
        LineTo(dc, cx + radius, cy + radius);
        MoveToEx(dc, cx + radius, cy - radius, nullptr);
        LineTo(dc, cx - radius, cy + radius);
        break;
    default:
        break;
    }
}

const wchar_t* TooltipText(ToolbarAction action) {
    switch (action) {
    case ToolbarAction::Rectangle: return L"矩形";
    case ToolbarAction::Arrow: return L"箭头";
    case ToolbarAction::Pen: return L"画笔";
    case ToolbarAction::Text: return L"文字";
    case ToolbarAction::Mosaic: return L"马赛克";
    case ToolbarAction::Undo: return L"撤销  Ctrl+Z";
    case ToolbarAction::Redo: return L"重做  Ctrl+Y";
    case ToolbarAction::Ocr: return L"文字识别  Ctrl+R";
    case ToolbarAction::Pin: return L"贴到屏幕  Ctrl+T";
    case ToolbarAction::Save: return L"保存  Ctrl+S";
    case ToolbarAction::Copy: return L"复制并完成  Enter";
    case ToolbarAction::Cancel: return L"取消  Esc";
    default: return L"";
    }
}

}  // namespace

void Toolbar::Update(RectI selectionClient, RectI desktopClient, UINT dpi) {
    buttonSize_ = MulDiv(38, static_cast<int>(dpi), 96);
    const int width = buttonSize_ * kButtonCount;
    const int gap = MulDiv(8, static_cast<int>(dpi), 96);
    int left = selectionClient.left;
    int top = selectionClient.bottom + gap;
    if (top + buttonSize_ > desktopClient.bottom) {
        top = selectionClient.top - buttonSize_ - gap;
    }
    if (top < desktopClient.top) {
        top = std::max(desktopClient.top, selectionClient.bottom - buttonSize_);
    }
    left = std::clamp(left, desktopClient.left, std::max(desktopClient.left, desktopClient.right - width));
    bounds_ = RectI{left, top, left + width, top + buttonSize_};
}

void Toolbar::Draw(HDC dc, AnnotationTool selectedTool, bool canUndo, bool canRedo,
                   ToolbarAction hoveredAction, bool showTooltip,
                   RectI desktopClient) const {
    if (bounds_.Empty()) {
        return;
    }
    UniqueBrush shadow(CreateSolidBrush(RGB(18, 19, 22)));
    SelectObjectGuard selectedShadow(dc, shadow.get());
    UniquePen shadowPen(CreatePen(PS_SOLID, 1, RGB(18, 19, 22)));
    SelectObjectGuard selectedShadowPen(dc, shadowPen.get());
    RoundRect(dc, bounds_.left + 3, bounds_.top + 4,
              bounds_.right + 3, bounds_.bottom + 4, 12, 12);

    UniqueBrush background(CreateSolidBrush(RGB(34, 36, 41)));
    SelectObjectGuard selectedBrush(dc, background.get());
    UniquePen border(CreatePen(PS_SOLID, 1, RGB(77, 80, 88)));
    SelectObjectGuard selectedPen(dc, border.get());
    RoundRect(dc, bounds_.left, bounds_.top, bounds_.right, bounds_.bottom, 12, 12);

    for (int index = 0; index < kButtonCount; ++index) {
        RECT button{bounds_.left + index * buttonSize_, bounds_.top,
                    bounds_.left + (index + 1) * buttonSize_, bounds_.bottom};
        const ToolbarAction action = kActions[static_cast<std::size_t>(index)];
        if (IsSelected(action, selectedTool)) {
            UniqueBrush selected(CreateSolidBrush(RGB(30, 118, 220)));
            SelectObjectGuard selectedHighlight(dc, selected.get());
            UniquePen selectedBorder(CreatePen(PS_SOLID, 1, RGB(55, 143, 238)));
            SelectObjectGuard selectedHighlightPen(dc, selectedBorder.get());
            RoundRect(dc, button.left + 3, button.top + 3,
                      button.right - 3, button.bottom - 3, 8, 8);
        } else if (action == hoveredAction) {
            UniqueBrush hovered(CreateSolidBrush(RGB(61, 64, 72)));
            SelectObjectGuard selectedHover(dc, hovered.get());
            UniquePen hoverBorder(CreatePen(PS_SOLID, 1, RGB(78, 82, 91)));
            SelectObjectGuard selectedHoverPen(dc, hoverBorder.get());
            RoundRect(dc, button.left + 3, button.top + 3,
                      button.right - 3, button.bottom - 3, 8, 8);
        }
        const bool enabled = (action != ToolbarAction::Undo || canUndo) &&
                             (action != ToolbarAction::Redo || canRedo);
        DrawIcon(dc, action, button, enabled);

        if (index == 4 || index == 6) {
            UniquePen separator(CreatePen(PS_SOLID, 1, RGB(77, 80, 88)));
            SelectObjectGuard selectedSeparator(dc, separator.get());
            MoveToEx(dc, button.right, button.top + 9, nullptr);
            LineTo(dc, button.right, button.bottom - 9);
        }
    }

    if (showTooltip && hoveredAction != ToolbarAction::None) {
        const wchar_t* text = TooltipText(hoveredAction);
        SIZE textSize{};
        GetTextExtentPoint32W(dc, text, lstrlenW(text), &textSize);
        const int paddingX = 10;
        const int tooltipHeight = textSize.cy + 10;
        const int tooltipWidth = textSize.cx + paddingX * 2;
        const int hoveredIndex = static_cast<int>(
            std::find(kActions.begin(), kActions.end(), hoveredAction) - kActions.begin());
        int left = bounds_.left + hoveredIndex * buttonSize_ +
                   (buttonSize_ - tooltipWidth) / 2;
        left = std::clamp(left, desktopClient.left,
                          std::max(desktopClient.left, desktopClient.right - tooltipWidth));
        int top = bounds_.top - tooltipHeight - 7;
        if (top < desktopClient.top) {
            top = bounds_.bottom + 7;
        }
        RECT tooltip{left, top, left + tooltipWidth, top + tooltipHeight};
        UniqueBrush tooltipBrush(CreateSolidBrush(RGB(24, 25, 29)));
        SelectObjectGuard selectedTooltip(dc, tooltipBrush.get());
        UniquePen tooltipPen(CreatePen(PS_SOLID, 1, RGB(83, 86, 94)));
        SelectObjectGuard selectedTooltipPen(dc, tooltipPen.get());
        RoundRect(dc, tooltip.left, tooltip.top, tooltip.right, tooltip.bottom, 8, 8);
        SetTextColor(dc, RGB(245, 246, 248));
        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, text, -1, &tooltip, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

ToolbarAction Toolbar::HitTest(POINT clientPoint) const {
    if (!bounds_.Contains(clientPoint)) {
        return ToolbarAction::None;
    }
    const int index = (clientPoint.x - bounds_.left) / buttonSize_;
    if (index < 0 || index >= kButtonCount) {
        return ToolbarAction::None;
    }
    return kActions[static_cast<std::size_t>(index)];
}

}  // namespace snaplite
