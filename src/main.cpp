#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

LRESULT CALLBACK win32_window_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param);

int WINAPI wWinMain(HINSTANCE instance, [[maybe_unused]] HINSTANCE prev_instance, [[maybe_unused]] PWSTR cmd_line,
                    [[maybe_unused]] int cmd_show)
{
    // Register the window class.
    constexpr wchar_t CLASS_NAME[] = L"Sample Window Class";

    const WNDCLASS wc = {
        .lpfnWndProc = win32_window_proc,
        .hInstance = instance,
        .lpszClassName = CLASS_NAME,
    };

    RegisterClass(&wc);

    // Create the window.
    const HWND window = CreateWindowEx(0, CLASS_NAME, L"nether-engine", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, instance, NULL);

    if (window == NULL)
    {
        return 0;
    }

    ShowWindow(window, SW_SHOW);

    // Run the message loop.

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK win32_window_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(window_handle, message, w_param, l_param);
}
