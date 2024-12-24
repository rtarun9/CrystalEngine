#ifndef UNICODE
#define UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <exception>
#include <iostream>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#ifdef DEF_NETHER_DEBUG
static constexpr bool NETHER_DEBUG = true;
#else
static constexpr bool NETHER_DEBUG = false;
#endif

static inline void throw_if_failed(const HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception();
    }
}

LRESULT CALLBACK win32_window_proc(const HWND window_handle, const UINT message, const WPARAM w_param,
                                   const LPARAM l_param);

int WINAPI wWinMain(HINSTANCE instance, [[maybe_unused]] HINSTANCE prev_instance, [[maybe_unused]] PWSTR cmd_line,
                    [[maybe_unused]] int cmd_show)
{
    constexpr uint32_t WINDOW_WIDTH = 1080u;
    constexpr uint32_t WINDOW_HEIGHT = 720u;

    // Register the window class.
    constexpr wchar_t WINDOW_CLASS_NAME[] = L"Base Window Class";

    const WNDCLASS window_class = {
        .lpfnWndProc = win32_window_proc,
        .hInstance = instance,
        .lpszClassName = WINDOW_CLASS_NAME,
    };

    RegisterClass(&window_class);

    // Create the window.
    const HWND window_handle =
        CreateWindowEx(0, WINDOW_CLASS_NAME, L"nether-engine", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                       WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, instance, NULL);

    if (window_handle == NULL)
    {
        return 0;
    }

    try
    {
        Microsoft::WRL::ComPtr<ID3D12Debug5> debug_device = {};

        // Enable the d3d12 debug layer.
        uint32_t dxgi_factory_creation_flags = 0u;

        if constexpr (NETHER_DEBUG)
        {
            throw_if_failed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_device)));
            debug_device->EnableDebugLayer();
            debug_device->SetEnableAutoName(true);
            debug_device->SetEnableGPUBasedValidation(true);
            debug_device->SetEnableSynchronizedCommandQueueValidation(true);

            dxgi_factory_creation_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        // Create dxgi factory to get access to swapchain and adapter.
        Microsoft::WRL::ComPtr<IDXGIFactory6> dxgi_factory = {};
        throw_if_failed(CreateDXGIFactory2(dxgi_factory_creation_flags, IID_PPV_ARGS(&dxgi_factory)));

        // Queyr adapter.
        Microsoft::WRL::ComPtr<IDXGIAdapter3> dxgi_adapter = {};
        throw_if_failed(dxgi_factory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                                 IID_PPV_ARGS(&dxgi_adapter)));

        // Create the d3d12 device, which is the way most objects are created in d3d12.
        Microsoft::WRL::ComPtr<ID3D12Device5> device = {};
        throw_if_failed(D3D12CreateDevice(dxgi_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));

        ShowWindow(window_handle, SW_SHOW);

        // Main game loop.
        bool quit = false;
        while (!quit)
        {
            MSG message = {};
            while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE))
            {
                switch (message.message)
                {
                case WM_SYSKEYDOWN:
                case WM_KEYDOWN: {
                    if (message.wParam == VK_ESCAPE)
                    {
                        quit = true;
                    }
                }
                break;
                }

                TranslateMessage(&message);
                DispatchMessage(&message);
            }
        }
    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
        return -1;
    }

    return 0;
}

LRESULT CALLBACK win32_window_proc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_DESTROY: {
        PostQuitMessage(0);
    }
    break;
    }

    return DefWindowProc(window_handle, message, w_param, l_param);
}
