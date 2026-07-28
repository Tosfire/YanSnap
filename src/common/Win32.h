#pragma once

#include <windows.h>

#include <utility>

namespace snaplite {

template <typename T, auto Deleter>
class UniqueHandle {
public:
    UniqueHandle() noexcept = default;
    explicit UniqueHandle(T value) noexcept : value_(value) {}
    ~UniqueHandle() { reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : value_(other.release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] T get() const noexcept { return value_; }
    [[nodiscard]] explicit operator bool() const noexcept { return value_ != T{}; }

    T release() noexcept {
        return std::exchange(value_, T{});
    }

    void reset(T replacement = T{}) noexcept {
        if (value_ != T{}) {
            Deleter(value_);
        }
        value_ = replacement;
    }

private:
    T value_{};
};

inline void DeleteGdiObject(HGDIOBJ value) {
    DeleteObject(value);
}

using UniqueBitmap = UniqueHandle<HBITMAP, DeleteGdiObject>;
using UniqueBrush = UniqueHandle<HBRUSH, DeleteGdiObject>;
using UniquePen = UniqueHandle<HPEN, DeleteGdiObject>;
using UniqueFont = UniqueHandle<HFONT, DeleteGdiObject>;
using UniqueIcon = UniqueHandle<HICON, DestroyIcon>;

class WindowDc {
public:
    WindowDc(HWND window, HDC dc) noexcept : window_(window), dc_(dc) {}
    ~WindowDc() {
        if (dc_) {
            ReleaseDC(window_, dc_);
        }
    }
    WindowDc(const WindowDc&) = delete;
    WindowDc& operator=(const WindowDc&) = delete;
    [[nodiscard]] HDC get() const noexcept { return dc_; }

private:
    HWND window_{};
    HDC dc_{};
};

class MemoryDc {
public:
    explicit MemoryDc(HDC compatible) : dc_(CreateCompatibleDC(compatible)) {}
    ~MemoryDc() {
        if (dc_) {
            DeleteDC(dc_);
        }
    }
    MemoryDc(const MemoryDc&) = delete;
    MemoryDc& operator=(const MemoryDc&) = delete;
    [[nodiscard]] HDC get() const noexcept { return dc_; }
    [[nodiscard]] explicit operator bool() const noexcept { return dc_ != nullptr; }

private:
    HDC dc_{};
};

class SelectObjectGuard {
public:
    SelectObjectGuard(HDC dc, HGDIOBJ object) : dc_(dc), previous_(SelectObject(dc, object)) {}
    ~SelectObjectGuard() {
        if (previous_ && previous_ != HGDI_ERROR) {
            SelectObject(dc_, previous_);
        }
    }
    SelectObjectGuard(const SelectObjectGuard&) = delete;
    SelectObjectGuard& operator=(const SelectObjectGuard&) = delete;

private:
    HDC dc_{};
    HGDIOBJ previous_{};
};

}  // namespace snaplite

