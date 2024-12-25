#ifndef UNICODE
#define UNICODE
#endif

// Windows includes.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl/client.h>

// Standard library includes.
#include <array>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <source_location>

// DXGI / D3D12 includes.
#include <d3d12.h>
#include <dxgi1_6.h>

// Typedefs for commonly used datatypes.
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

#ifdef DEF_NETHER_DEBUG
static constexpr bool NETHER_DEBUG = true;
#else
static constexpr bool NETHER_DEBUG = false;
#endif

static inline void throw_if_failed(const HRESULT hr,
                                   const std::source_location source_location = std::source_location::current())
{
    if (FAILED(hr))
    {
        throw std::runtime_error(std::format("Exception caught at :: File name {}, Function name {}, Line number {}",
                                             source_location.file_name(), source_location.function_name(),
                                             source_location.line()));
    }
}

LRESULT CALLBACK win32_window_proc(const HWND window_handle, const UINT message, const WPARAM w_param,
                                   const LPARAM l_param);

int main()
{
    HINSTANCE instance = GetModuleHandle(nullptr);

    constexpr u32 WINDOW_WIDTH = 1080u;
    constexpr u32 WINDOW_HEIGHT = 720u;

    constexpr u32 NUM_BACK_BUFFERS = 2u;

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

    // Get client dimensions from the window.
    D3D12_RECT client_rect = {};
    GetClientRect(window_handle, &client_rect);

    const u32 CLIENT_WIDTH = client_rect.right - client_rect.left;
    const u32 CLIENT_HEIGHT = client_rect.bottom - client_rect.top;

    try
    {
        Microsoft::WRL::ComPtr<ID3D12Debug5> debug_layer = {};

        // Enable the d3d12 debug layer in debug mode.
        uint32_t dxgi_factory_creation_flags = 0u;

        if constexpr (NETHER_DEBUG)
        {
            throw_if_failed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_layer)));

            debug_layer->EnableDebugLayer();
            debug_layer->SetEnableAutoName(true);
            debug_layer->SetEnableGPUBasedValidation(true);
            debug_layer->SetEnableSynchronizedCommandQueueValidation(true);

            dxgi_factory_creation_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }

        // Create dxgi factory to get access to dxgi objects (like the swapchain and adapter).
        Microsoft::WRL::ComPtr<IDXGIFactory6> dxgi_factory = {};
        throw_if_failed(CreateDXGIFactory2(dxgi_factory_creation_flags, IID_PPV_ARGS(&dxgi_factory)));

        // Query the adapter (interface to the actual GPU).
        Microsoft::WRL::ComPtr<IDXGIAdapter3> dxgi_adapter = {};
        throw_if_failed(dxgi_factory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                                 IID_PPV_ARGS(&dxgi_adapter)));

        // Display information about the chosen adapter.
        DXGI_ADAPTER_DESC adapter_desc = {};
        throw_if_failed(dxgi_adapter->GetDesc(&adapter_desc));
        std::wcout << std::format(L"Chosen adapter description :: {}", adapter_desc.Description) << std::endl;

        // Create the d3d12 device, which is required for creation of most objects in d3d12.
        Microsoft::WRL::ComPtr<ID3D12Device5> device = {};
        throw_if_failed(D3D12CreateDevice(dxgi_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));

        // Setup a info queue, so that breakpoints can be set when a message severity of a specific type comes up.
        Microsoft::WRL::ComPtr<ID3D12InfoQueue> info_queue = {};
        if constexpr (NETHER_DEBUG)
        {
            throw_if_failed(device.As(&info_queue));

            throw_if_failed(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
            throw_if_failed(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true));
            throw_if_failed(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
        }

        // Query a d3d12 debug device to make sure all objects are properly cleared up and are not live at end of
        // application.
        Microsoft::WRL::ComPtr<ID3D12DebugDevice2> debug_device = {};

        if constexpr (NETHER_DEBUG)
        {
            throw_if_failed(device->QueryInterface(IID_PPV_ARGS(&debug_device)));
        }

        // Create the command queue, the execution port of GPU's.
        const D3D12_COMMAND_QUEUE_DESC direct_command_queue_desc = {

            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
            .Priority = 0u,
            .Flags = D3D12_COMMAND_QUEUE_FLAGS::D3D12_COMMAND_QUEUE_FLAG_NONE,
            .NodeMask = 0u,
        };

        Microsoft::WRL::ComPtr<ID3D12CommandQueue> direct_command_queue = {};
        throw_if_failed(device->CreateCommandQueue(&direct_command_queue_desc, IID_PPV_ARGS(&direct_command_queue)));

        // Create the command allocators (i.e backing store for commands recorded via command lists).
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> direct_command_allocator = {};
        throw_if_failed(
            device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&direct_command_allocator)));

        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> graphics_command_list = {};
        throw_if_failed(device->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_DIRECT, direct_command_allocator.Get(),
                                                  nullptr, IID_PPV_ARGS(&graphics_command_list)));

        // Create sync primitives.
        Microsoft::WRL::ComPtr<ID3D12Fence> fence = {};
        throw_if_failed(device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

        std::array<u64, NUM_BACK_BUFFERS> frame_fence_values = {};
        u64 current_fence_value = 0u;

        // Create the swapchain.
        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapchain = {};
        const DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {
            .Width = CLIENT_WIDTH,
            .Height = CLIENT_HEIGHT,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .Stereo = false,
            .SampleDesc = {1u, 0u},
            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
            .BufferCount = NUM_BACK_BUFFERS,
            .Scaling = DXGI_SCALING_NONE,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE::DXGI_ALPHA_MODE_IGNORE,
            .Flags = 0u,
        };

        throw_if_failed(dxgi_factory->CreateSwapChainForHwnd(direct_command_queue.Get(), window_handle, &swapchain_desc,
                                                             nullptr, nullptr, &swapchain));

        // Create RTV descriptor heap, which is a contiguous memory allocation for render target views, which describe a
        // particular resource.
        const D3D12_DESCRIPTOR_HEAP_DESC rtv_descriptor_heap_desc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = NUM_BACK_BUFFERS,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0u,
        };
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_descriptor_heap = {};
        throw_if_failed(device->CreateDescriptorHeap(&rtv_descriptor_heap_desc, IID_PPV_ARGS(&rtv_descriptor_heap)));

        // Create RTV for each of the swapchain backbuffer image.

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
