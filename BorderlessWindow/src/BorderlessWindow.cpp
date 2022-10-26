#include "BorderLessWindow.hpp"

#include <string>
#include <stdexcept>
#include <system_error>

#include <windowsx.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")


namespace {
    enum class Style : DWORD {
        aero_borderless = WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX
    };

    bool maximized(HWND hwnd) {
        WINDOWPLACEMENT placement{};
        if (!::GetWindowPlacement(hwnd, &placement)) {
            return false;
        }

        return placement.showCmd == SW_MAXIMIZE;
    }

    /* Adjust client rect to not spill over monitor edges when maximized.
     * rect(in/out): in: proposed window rect, out: calculated client rect
     * Does nothing if the window is not maximized.
     */
    void adjust_maximized_client_rect(HWND window, RECT& rect) {
        if (!maximized(window)) {
            return;
        }

        // Values copied from Chromium
        rect.left += 7;
        rect.top += 7;
        // If you put -8 here, a 1px gap disappears on Windows 11
        // but on Windows 10 you won't be able to trigger the hidden taskbar to appear
        // by dragging the mouse to the bottom of the screen
        rect.bottom -= 9;
        rect.right -= 7;
    }

    std::system_error last_error(const std::string& message) {
        return std::system_error(
            std::error_code(::GetLastError(), std::system_category()),
            message
        );
    }

    const wchar_t* window_class(WNDPROC wndproc) {
        static const wchar_t* window_class_name = [&] {
            WNDCLASSEXW wcx{};
            wcx.cbSize = sizeof(wcx);
            wcx.style = CS_HREDRAW | CS_VREDRAW;
            wcx.hInstance = nullptr;
            wcx.lpfnWndProc = wndproc;
            wcx.lpszClassName = L"BorderlessWindowClass";
            wcx.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0, 120, 212));
            wcx.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
            const ATOM result = ::RegisterClassExW(&wcx);
            if (!result) {
                throw last_error("failed to register window class");
            }
            return wcx.lpszClassName;
        }();
        return window_class_name;
    }

    bool composition_enabled() {
        BOOL composition_enabled = FALSE;
        bool success = ::DwmIsCompositionEnabled(&composition_enabled) == S_OK;
        return composition_enabled && success;
    }

    void set_shadow(HWND handle) {
        if (composition_enabled()) {
            static const MARGINS margins{ 0,0,1,0 };
            ::DwmExtendFrameIntoClientArea(handle, &margins);
        }
    }

    unique_handle create_window(WNDPROC wndproc, void* userdata) {
        auto handle = CreateWindowExW(
            0, window_class(wndproc), L"Borderless Window",
            static_cast<DWORD>(Style::aero_borderless), CW_USEDEFAULT, CW_USEDEFAULT,
            1280, 720, nullptr, nullptr, nullptr, userdata
        );
        if (!handle) {
            throw last_error("failed to create window");
        }
        return unique_handle{ handle };
    }
}

BorderlessWindow::BorderlessWindow()
    : handle{ create_window(&BorderlessWindow::WndProc, this) }
{
    set_borderless();
    ::ShowWindow(handle.get(), SW_SHOW);
}

void BorderlessWindow::set_borderless() {
    Style new_style = Style::aero_borderless;

    ::SetWindowLongPtrW(handle.get(), GWL_STYLE, static_cast<LONG>(new_style));

    set_shadow(handle.get());

    // redraw frame
    ::SetWindowPos(handle.get(), nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
    ::ShowWindow(handle.get(), SW_SHOW);
}

auto CALLBACK BorderlessWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept -> LRESULT {
    if (msg == WM_NCCREATE) {
        auto userdata = reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams;
        // store window instance pointer in window user data
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(userdata));
    }
    if (auto window_ptr = reinterpret_cast<BorderlessWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA))) {
        auto& window = *window_ptr;

        switch (msg) {
        case WM_NCCALCSIZE: {
            if (wparam == TRUE) {
                auto& params = *reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
                adjust_maximized_client_rect(hwnd, params.rgrc[0]);
                return 0;
            }
            break;
        }
        case WM_NCHITTEST: {
            // When we have no border or title bar, we need to perform our
            // own hit testing to allow resizing and moving.
            return window.hit_test(POINT{
                GET_X_LPARAM(lparam),
                GET_Y_LPARAM(lparam)
                });
        }
        case WM_NCACTIVATE: {
            if (!composition_enabled()) {
                // Prevents window frame reappearing on window activation
                // in "basic" theme, where no aero shadow is present.
                return 1;
            }
            break;
        }

        case WM_CLOSE: {
            ::DestroyWindow(hwnd);
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
        }
    }

    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

auto BorderlessWindow::hit_test(POINT cursor) const -> LRESULT {
    // identify borders and corners to allow resizing the window.
    // Note: On Windows 10, windows behave differently and
    // allow resizing outside the visible window frame.
    // This implementation does not replicate that behavior.
    const POINT border{
        ::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER),
        ::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)
    };
    RECT window;
    if (!::GetWindowRect(handle.get(), &window)) {
        return HTNOWHERE;
    }

    const auto drag = HTCAPTION;

    enum region_mask {
        client = 0b0000,
        left = 0b0001,
        right = 0b0010,
        top = 0b0100,
        bottom = 0b1000,
    };

    const auto result =
        left * (cursor.x < (window.left + border.x)) |
        right * (cursor.x >= (window.right - border.x)) |
        top * (cursor.y < (window.top + border.y)) |
        bottom * (cursor.y >= (window.bottom - border.y));

    switch (result) {
    case left: return HTLEFT;
    case right: return HTRIGHT;
    case top: return HTTOP;
    case bottom: return HTBOTTOM;
    case top | left: return HTTOPLEFT;
    case top | right: return HTTOPRIGHT;
    case bottom | left: return HTBOTTOMLEFT;
    case bottom | right: return HTBOTTOMRIGHT;
    case client: return drag;
    default: return HTNOWHERE;
    }
}
