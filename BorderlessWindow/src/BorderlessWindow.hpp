#pragma once

#include <memory>

#include <Windows.h>

struct hwnd_deleter {
    using pointer = HWND;
    auto operator()(HWND handle) const -> void {
        ::DestroyWindow(handle);
    }
};

using unique_handle = std::unique_ptr<HWND, hwnd_deleter>;

class BorderlessWindow {
public:
    BorderlessWindow();
    void set_borderless();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept;
    auto hit_test(POINT cursor) const -> LRESULT;

    unique_handle handle;
};
