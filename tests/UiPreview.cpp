#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cstring>
#include <string_view>

#include "common/Win32.h"
#include "export/ImageData.h"
#include "export/PngEncoder.h"
#include "overlay/Toolbar.h"

namespace {

snaplite::Toolbar g_toolbar;
snaplite::ToolbarAction g_hovered = snaplite::ToolbarAction::Ocr;

void DrawPreview(HDC dc, const RECT& client) {
    snaplite::UniqueBrush backdrop(CreateSolidBrush(RGB(21, 25, 33)));
    FillRect(dc, &client, backdrop.get());

    // A neutral fake desktop keeps the preview deterministic.
    for (int y = 0; y < client.bottom; y += 36) {
        const int shade = 42 + (y / 36) % 5 * 6;
        snaplite::UniqueBrush band(CreateSolidBrush(RGB(shade, shade + 5, shade + 12)));
        RECT strip{0, y, client.right,
                   std::min<int>(static_cast<int>(client.bottom), y + 36)};
        FillRect(dc, &strip, band.get());
    }

    const snaplite::RectI selection{140, 82, 940, 552};
    snaplite::UniqueBrush panel(CreateSolidBrush(RGB(235, 239, 246)));
    RECT selected = selection.ToRect();
    FillRect(dc, &selected, panel.get());

    snaplite::UniqueBrush titleBar(CreateSolidBrush(RGB(32, 137, 255)));
    RECT title{selection.left, selection.top, selection.right, selection.top + 64};
    FillRect(dc, &title, titleBar.get());
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    RECT titleText{selection.left + 24, selection.top + 18,
                   selection.right - 24, selection.top + 52};
    DrawTextW(dc, L"YanSnap 选区界面预览", -1, &titleText,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    snaplite::UniqueBrush card(CreateSolidBrush(RGB(255, 255, 255)));
    RECT cardOne{selection.left + 30, selection.top + 96,
                 selection.left + 360, selection.bottom - 38};
    RECT cardTwo{selection.left + 390, selection.top + 96,
                 selection.right - 30, selection.bottom - 38};
    FillRect(dc, &cardOne, card.get());
    FillRect(dc, &cardTwo, card.get());

    snaplite::UniquePen border(CreatePen(PS_SOLID, 2, RGB(32, 137, 255)));
    snaplite::SelectObjectGuard selectedBorder(dc, border.get());
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, selection.left, selection.top, selection.right, selection.bottom);
    SelectObject(dc, oldBrush);

    snaplite::UniqueBrush handleBrush(CreateSolidBrush(RGB(32, 137, 255)));
    snaplite::SelectObjectGuard selectedHandleBrush(dc, handleBrush.get());
    snaplite::UniquePen handlePen(CreatePen(PS_SOLID, 1, RGB(255, 255, 255)));
    snaplite::SelectObjectGuard selectedHandlePen(dc, handlePen.get());
    const int centerX = (selection.left + selection.right) / 2;
    const int centerY = (selection.top + selection.bottom) / 2;
    const POINT handles[8] = {
        {selection.left, selection.top}, {centerX, selection.top},
        {selection.right, selection.top}, {selection.left, centerY},
        {selection.right, centerY}, {selection.left, selection.bottom},
        {centerX, selection.bottom}, {selection.right, selection.bottom},
    };
    for (POINT handle : handles) {
        Ellipse(dc, handle.x - 4, handle.y - 4, handle.x + 5, handle.y + 5);
    }

    RECT sizeLabel{selection.left, selection.top - 33, selection.left + 105,
                   selection.top - 7};
    snaplite::UniqueBrush labelBrush(CreateSolidBrush(RGB(32, 35, 40)));
    snaplite::SelectObjectGuard selectedLabel(dc, labelBrush.get());
    snaplite::UniquePen labelPen(CreatePen(PS_SOLID, 1, RGB(75, 79, 87)));
    snaplite::SelectObjectGuard selectedLabelPen(dc, labelPen.get());
    RoundRect(dc, sizeLabel.left, sizeLabel.top, sizeLabel.right,
              sizeLabel.bottom, 8, 8);
    SetTextColor(dc, RGB(245, 247, 250));
    DrawTextW(dc, L"800 × 470", -1, &sizeLabel,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    g_toolbar.Update(selection, snaplite::RectI{0, 0, client.right, client.bottom}, 96);
    g_toolbar.Draw(dc, snaplite::AnnotationTool::Rectangle, true, true,
                   g_hovered, g_hovered != snaplite::ToolbarAction::None,
                   snaplite::RectI{0, 0, client.right, client.bottom});
}

void PaintPreview(HWND window) {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    RECT client{};
    GetClientRect(window, &client);
    DrawPreview(dc, client);
    EndPaint(window, &paint);
}

bool RenderPreview(const wchar_t* outputPath) {
    constexpr int width = 1120;
    constexpr int height = 720;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bitmap =
        CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!dc || !bitmap || !pixels) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        if (dc) {
            DeleteDC(dc);
        }
        return false;
    }

    HGDIOBJ previous = SelectObject(dc, bitmap);
    DrawPreview(dc, RECT{0, 0, width, height});

    snaplite::ImageData image;
    image.width = width;
    image.height = height;
    image.stride = width * 4;
    image.pixels.resize(static_cast<std::size_t>(image.stride) * height);
    std::memcpy(image.pixels.data(), pixels, image.pixels.size());
    std::wstring error;
    const bool saved = snaplite::PngEncoder::Save(outputPath, image, &error);
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return saved;
}

LRESULT CALLBACK PreviewProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT:
        PaintPreview(window);
        return 0;
    case WM_MOUSEMOVE: {
        const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const auto hovered = g_toolbar.HitTest(point);
        if (hovered != g_hovered) {
            g_hovered = hovered;
            InvalidateRect(window, nullptr, FALSE);
        }
        SetCursor(LoadCursorW(nullptr,
                             hovered == snaplite::ToolbarAction::None ? IDC_ARROW : IDC_HAND));
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

}  // namespace

int wmain(int argumentCount, wchar_t** arguments) {
    if (argumentCount == 3 &&
        std::wstring_view(arguments[1]) == L"--render") {
        return RenderPreview(arguments[2]) ? 0 : 1;
    }

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = PreviewProc;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = L"YanSnap.UiPreview";
    RegisterClassW(&windowClass);
    HWND window = CreateWindowW(windowClass.lpszClassName, L"YanSnap UI Preview",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                1120, 760, nullptr, nullptr, instance, nullptr);
    if (!window) {
        return 1;
    }
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
