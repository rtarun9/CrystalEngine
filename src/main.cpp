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
#include <string>

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

using namespace Microsoft::WRL;

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

static inline void set_name_d3d12_object(ComPtr<ID3D12Object> object, const std::wstring_view name)
{
    if constexpr (NETHER_DEBUG)
    {
        throw_if_failed(object->SetName(name.data()));
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
        ComPtr<ID3D12Debug5> debug_layer = {};

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
        ComPtr<IDXGIFactory6> dxgi_factory = {};
        throw_if_failed(CreateDXGIFactory2(dxgi_factory_creation_flags, IID_PPV_ARGS(&dxgi_factory)));

        // Query the adapter (interface to the actual GPU).
        ComPtr<IDXGIAdapter3> dxgi_adapter = {};
        throw_if_failed(dxgi_factory->EnumAdapterByGpuPreference(0u, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                                 IID_PPV_ARGS(&dxgi_adapter)));

        // Display information about the chosen adapter.
        DXGI_ADAPTER_DESC adapter_desc = {};
        throw_if_failed(dxgi_adapter->GetDesc(&adapter_desc));
        std::wcout << std::format(L"Chosen adapter description :: {}", adapter_desc.Description) << std::endl;

        // Create the d3d12 device, which is required for creation of most objects in d3d12.
        ComPtr<ID3D12Device5> device = {};
        throw_if_failed(D3D12CreateDevice(dxgi_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));
        set_name_d3d12_object(device.Get(), L"D3D12 device");

        // Setup a info queue, so that breakpoints can be set when a message severity of a specific type comes up.
        ComPtr<ID3D12InfoQueue> info_queue = {};
        if constexpr (NETHER_DEBUG)
        {
            throw_if_failed(device.As(&info_queue));

            throw_if_failed(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
            throw_if_failed(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true));
            throw_if_failed(info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
        }

        // Query a d3d12 debug device to make sure all objects are properly cleared up and are not live at end of
        // application.
        ComPtr<ID3D12DebugDevice2> debug_device = {};

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

        ComPtr<ID3D12CommandQueue> direct_command_queue = {};
        throw_if_failed(device->CreateCommandQueue(&direct_command_queue_desc, IID_PPV_ARGS(&direct_command_queue)));
        set_name_d3d12_object(direct_command_queue.Get(), L"D3D12 direct command queue");

        // Create the command allocators (i.e backing store for commands recorded via command lists).
        std::array<ComPtr<ID3D12CommandAllocator>, NUM_BACK_BUFFERS> direct_command_allocators = {};
        for (u32 i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            throw_if_failed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                           IID_PPV_ARGS(&direct_command_allocators[i])));
            set_name_d3d12_object(direct_command_allocators[i].Get(),
                                  L"D3D12 direct command allocator" + std::to_wstring(i));
        }

        // Create command list.
        ComPtr<ID3D12GraphicsCommandList> graphics_command_list = {};
        throw_if_failed(device->CreateCommandList(0u, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                  direct_command_allocators[0].Get(), nullptr,
                                                  IID_PPV_ARGS(&graphics_command_list)));
        throw_if_failed(graphics_command_list->Close());
        set_name_d3d12_object(graphics_command_list.Get(), L"D3D12 Graphics command list");

        // Create sync primitives.
        ComPtr<ID3D12Fence> fence = {};
        throw_if_failed(device->CreateFence(0u, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        set_name_d3d12_object(fence.Get(), L"D3D12 direct command queue fence");

        std::array<u64, NUM_BACK_BUFFERS> frame_fence_values = {};
        u64 current_fence_value = 0u;

        // Create the swapchain.
        ComPtr<IDXGISwapChain1> swapchain_1 = {};
        const DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {
            .Width = CLIENT_WIDTH,
            .Height = CLIENT_HEIGHT,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
            .Stereo = false,
            .SampleDesc = {1u, 0u},
            .BufferUsage = DXGI_USAGE_BACK_BUFFER,
            .BufferCount = NUM_BACK_BUFFERS,
            .Scaling = DXGI_SCALING_NONE,
            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
            .AlphaMode = DXGI_ALPHA_MODE::DXGI_ALPHA_MODE_IGNORE,
            .Flags = 0u,
        };

        throw_if_failed(dxgi_factory->CreateSwapChainForHwnd(direct_command_queue.Get(), window_handle, &swapchain_desc,
                                                             nullptr, nullptr, &swapchain_1));

        ComPtr<IDXGISwapChain3> swapchain = {};
        throw_if_failed(swapchain_1.As(&swapchain));

        // Setup viewport and scissor rect.
        const D3D12_VIEWPORT viewport = {
            .TopLeftX = 0.0f,
            .TopLeftY = 0.0f,
            .Width = (f32)CLIENT_WIDTH,
            .Height = (f32)CLIENT_HEIGHT,
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        };

        const D3D12_RECT scissor_rect = {
            .left = 0,
            .top = 0,
            .right = (LONG)CLIENT_WIDTH,
            .bottom = (LONG)CLIENT_HEIGHT,
        };

        // Create RTV descriptor heap, which is a contiguous memory allocation for render target views, which describe a
        // particular resource.
        const D3D12_DESCRIPTOR_HEAP_DESC rtv_descriptor_heap_desc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = NUM_BACK_BUFFERS,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0u,
        };
        ComPtr<ID3D12DescriptorHeap> rtv_descriptor_heap = {};
        throw_if_failed(device->CreateDescriptorHeap(&rtv_descriptor_heap_desc, IID_PPV_ARGS(&rtv_descriptor_heap)));
        set_name_d3d12_object(rtv_descriptor_heap.Get(), L"D3D12 rtv descriptor heap");

        // Create RTV for each of the swapchain backbuffer image.
        struct BackBuffer
        {
            ComPtr<ID3D12Resource> resource{};
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_rtv_handle{};
        };
        std::array<BackBuffer, NUM_BACK_BUFFERS> back_buffers = {};

        D3D12_CPU_DESCRIPTOR_HANDLE current_rtv_descriptor_handle =
            rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart();

        SIZE_T rtv_descriptor_handle_increment_size =
            device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        for (u32 i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            throw_if_failed(swapchain->GetBuffer(i, IID_PPV_ARGS(&back_buffers[i].resource)));

            device->CreateRenderTargetView(back_buffers[i].resource.Get(), nullptr, current_rtv_descriptor_handle);

            back_buffers[i].cpu_rtv_handle = current_rtv_descriptor_handle;

            current_rtv_descriptor_handle.ptr += rtv_descriptor_handle_increment_size;
        }

        ShowWindow(window_handle, SW_SHOW);

        // Main game loop.
        u32 current_swapchain_backbuffer_index = swapchain->GetCurrentBackBufferIndex();

        u64 frame_index = 0u;
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

            // Reset command allocator and list for current frame.
            throw_if_failed(direct_command_allocators[current_swapchain_backbuffer_index]->Reset());
            throw_if_failed(graphics_command_list->Reset(
                direct_command_allocators[current_swapchain_backbuffer_index].Get(), nullptr));

            graphics_command_list->OMSetRenderTargets(
                1u, &back_buffers[current_swapchain_backbuffer_index].cpu_rtv_handle, false, nullptr);

            // Transition the swapchain backbuffer from a presentable format to render target view.
            const D3D12_RESOURCE_BARRIER backbuffer_present_to_render_target = {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Transition =
                    {

                        .pResource = back_buffers[current_swapchain_backbuffer_index].resource.Get(),
                        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
                        .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET,
                    },
            };
            graphics_command_list->ResourceBarrier(1u, &backbuffer_present_to_render_target);

            const std::array<f32, 4> clear_color{sinf((f32)frame_index / 120.0f), 0.0f, 0.0f, 1.0f};

            graphics_command_list->ClearRenderTargetView(
                back_buffers[current_swapchain_backbuffer_index].cpu_rtv_handle, clear_color.data(), 0u, nullptr);
            graphics_command_list->RSSetViewports(1u, &viewport);
            graphics_command_list->RSSetScissorRects(1u, &scissor_rect);

            // Transition swapchain back to presentable format.
            const D3D12_RESOURCE_BARRIER backbuffer_render_target_to_present = {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Transition =
                    {

                        .pResource = back_buffers[current_swapchain_backbuffer_index].resource.Get(),
                        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
                        .StateAfter = D3D12_RESOURCE_STATE_PRESENT,
                    },
            };
            graphics_command_list->ResourceBarrier(1u, &backbuffer_render_target_to_present);

            // Submit command list for execution.
            throw_if_failed(graphics_command_list->Close());

            ID3D12CommandList *const command_lists_to_execute[] = {
                graphics_command_list.Get(),
            };

            direct_command_queue->ExecuteCommandLists(1u, command_lists_to_execute);

            // Present & signal.
            throw_if_failed(swapchain->Present(1u, 0u));

            current_fence_value++;
            throw_if_failed(direct_command_queue->Signal(fence.Get(), current_fence_value));
            frame_fence_values[current_swapchain_backbuffer_index] = current_fence_value;

            current_swapchain_backbuffer_index = swapchain->GetCurrentBackBufferIndex();
            if (fence->GetCompletedValue() < frame_fence_values[current_swapchain_backbuffer_index])
            {
                fence->SetEventOnCompletion(frame_fence_values[current_swapchain_backbuffer_index], nullptr);
            }

            ++frame_index;
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
