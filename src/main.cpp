#include "common.hpp"

#include "descriptor_heap.hpp"
#include "shader_compiler.hpp"

// Parameter setup for directx agility SDK.
extern "C"
{
    __declspec(dllexport) extern const UINT D3D12SDKVersion = 614u;
}
extern "C"
{
    __declspec(dllexport) extern const char *D3D12SDKPath = "D3D12//";
}

LRESULT CALLBACK win32_window_proc(const HWND window_handle, const UINT message, const WPARAM w_param,
                                   const LPARAM l_param);

struct upload_buffer_creation_result_t
{
    u8 *ptr{};
    ComPtr<ID3D12Resource> resource{};
    u64 srv_index{};
};

// Create upload buffer, copies data to it, and creates a SRV.
template <typename T>
upload_buffer_creation_result_t create_upload_buffer(ID3D12Device *const device, const std::span<const T> data,
                                                     nether::descriptor_heap_t *const cbv_srv_uav_descriptor_heap)
{
    upload_buffer_creation_result_t result{};

    const D3D12_HEAP_PROPERTIES upload_heap_properties = {
        .Type = D3D12_HEAP_TYPE_UPLOAD,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0u,
        .VisibleNodeMask = 0u,
    };

    const D3D12_RESOURCE_DESC buffer_resource_desc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0u,
        .Width = data.size_bytes(),
        .Height = 1u,
        .DepthOrArraySize = 1u,
        .MipLevels = 1u,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1u, 0u},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };

    throw_if_failed(device->CreateCommittedResource(&upload_heap_properties, D3D12_HEAP_FLAG_NONE,
                                                    &buffer_resource_desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                    IID_PPV_ARGS(&result.resource)));

    const D3D12_RANGE no_read_range = {
        .Begin = 0u,
        .End = 0u,
    };

    result.resource->Map(0u, &no_read_range, (void **)&result.ptr);
    std::memcpy(result.ptr, data.data(), data.size_bytes());

    // create the SRV.
    const D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Buffer =
            {

                .FirstElement = 0u,
                .NumElements = static_cast<UINT>(data.size()),
                .StructureByteStride = sizeof(T),
                .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
            },
    };

    nether::descriptor_handle_t descriptor_handle =
        cbv_srv_uav_descriptor_heap->get_then_offset_current_descriptor_handle();
    device->CreateShaderResourceView(result.resource.Get(), &srv_desc, descriptor_handle.cpu_handle);

    result.srv_index = descriptor_handle.index;

    return result;
}

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
        nether::descriptor_heap_t rtv_descriptor_heap{device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_BACK_BUFFERS,
                                                      L"RTV Descriptor Heap"};

        nether::descriptor_heap_t cbv_srv_uav_descriptor_heap{device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 10u,
                                                              L"CBV SRV UAV Descriptor Heap"};

        // Create RTV for each of the swapchain backbuffer image.
        struct BackBuffer
        {
            ComPtr<ID3D12Resource> resource{};
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_rtv_handle{};
        };
        std::array<BackBuffer, NUM_BACK_BUFFERS> back_buffers = {};

        for (u32 i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            throw_if_failed(swapchain->GetBuffer(i, IID_PPV_ARGS(&back_buffers[i].resource)));

            nether::descriptor_handle_t descriptor_handle =
                rtv_descriptor_heap.get_then_offset_current_descriptor_handle();

            device->CreateRenderTargetView(back_buffers[i].resource.Get(), nullptr, descriptor_handle.cpu_handle);

            back_buffers[i].cpu_rtv_handle = descriptor_handle.cpu_handle;
        }

        // Create the root signature, which is kind of a function signature for shaders that descripts the shader's
        // inputs.
        ComPtr<ID3D12RootSignature> root_signature{};

        const D3D12_ROOT_PARAMETER1 root_parameter_desc = {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants =
                {

                    .ShaderRegister = 0u,
                    .RegisterSpace = 0u,
                    .Num32BitValues = 64,
                },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        };

        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = {

            .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
            .Desc_1_1 =
                {

                    .NumParameters = 1u,
                    .pParameters = &root_parameter_desc,
                    .NumStaticSamplers = 0u,
                    .Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED,
                },
        };

        ComPtr<ID3DBlob> root_signature_blob{};
        ComPtr<ID3DBlob> error_blob{};

        // Why do root signatures have this funky logic where you have to serialize a root signature first?
        // Simple, root signatures can be specified as a shader, and the compiled shader blob can be used as a
        // serialized root signature.
        throw_if_failed(D3D12SerializeVersionedRootSignature(&root_signature_desc, &root_signature_blob, &error_blob));
        throw_if_failed(device->CreateRootSignature(0u, root_signature_blob->GetBufferPointer(),
                                                    root_signature_blob->GetBufferSize(),
                                                    IID_PPV_ARGS(&root_signature)));

        // Create upload heaps for vertex data (position / color) and index buffer.
        static constexpr std::array<DirectX::XMFLOAT3, 3> position_data = {
            DirectX::XMFLOAT3{-0.5f, 0.5f, 0.0f},
            DirectX::XMFLOAT3{0.5f, 0.5f, 0.0f},
            DirectX::XMFLOAT3{0.0f, -0.5f, 0.0f},
        };

        upload_buffer_creation_result_t vertex_position_buffer_creation_result =
            create_upload_buffer<DirectX::XMFLOAT3>(device.Get(), position_data, &cbv_srv_uav_descriptor_heap);

        ComPtr<ID3D12Resource> vertex_color_buffer_resource{};
        static constexpr std::array<DirectX::XMFLOAT3, 3> color_data = {
            DirectX::XMFLOAT3{0.0f, 1.0f, 1.0f},
            DirectX::XMFLOAT3{1.0f, 0.0f, 1.0f},
            DirectX::XMFLOAT3{1.0f, 1.0f, 0.0f},
        };

        upload_buffer_creation_result_t vertex_color_buffer_creation_result =
            create_upload_buffer<DirectX::XMFLOAT3>(device.Get(), color_data, &cbv_srv_uav_descriptor_heap);

        static constexpr std::array<u16, 3> index_buffer_data = {0u, 1u, 2u};
        upload_buffer_creation_result_t index_buffer_creation_result =
            create_upload_buffer<u16>(device.Get(), index_buffer_data, &cbv_srv_uav_descriptor_heap);

        const D3D12_INDEX_BUFFER_VIEW index_buffer_view = {
            .BufferLocation = index_buffer_creation_result.resource->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(index_buffer_data.size() * sizeof(u16)),
            .Format = DXGI_FORMAT_R16_UINT,
        };

        // Compile shaders.
        ComPtr<IDxcBlob> vertex_shader_blob =
            nether::shader_compiler::compile_shader(L"shaders/triangle_shader.hlsl", L"vs_6_6", L"vs_main");
        ComPtr<IDxcBlob> pixel_shader_blob =
            nether::shader_compiler::compile_shader(L"shaders/triangle_shader.hlsl", L"ps_6_6", L"ps_main");

        const D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics_pipeline_state_desc = {
            .pRootSignature = root_signature.Get(),
            .VS =
                {
                    .pShaderBytecode = vertex_shader_blob->GetBufferPointer(),
                    .BytecodeLength = vertex_shader_blob->GetBufferSize(),
                },
            .PS =
                {
                    .pShaderBytecode = pixel_shader_blob->GetBufferPointer(),
                    .BytecodeLength = pixel_shader_blob->GetBufferSize(),
                },
            .BlendState =
                {
                    .AlphaToCoverageEnable = false,
                    .IndependentBlendEnable = false,
                    .RenderTarget = {D3D12_RENDER_TARGET_BLEND_DESC{
                        .BlendEnable = FALSE,
                        .LogicOpEnable = FALSE,
                        .SrcBlend = D3D12_BLEND_ONE,
                        .DestBlend = D3D12_BLEND_ZERO,
                        .BlendOp = D3D12_BLEND_OP_ADD,
                        .SrcBlendAlpha = D3D12_BLEND_ONE,
                        .DestBlendAlpha = D3D12_BLEND_ZERO,
                        .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                        .LogicOp = D3D12_LOGIC_OP_NOOP,
                        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
                    }},
                },
            .SampleMask = UINT_MAX,
            .RasterizerState =
                {
                    .FillMode = D3D12_FILL_MODE_SOLID,
                    .CullMode = D3D12_CULL_MODE_BACK,
                    .FrontCounterClockwise = FALSE,
                    .DepthClipEnable = FALSE,
                    .MultisampleEnable = FALSE,
                    .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
                },
            .DepthStencilState =
                {
                    .DepthEnable = FALSE,
                },
            .InputLayout =
                {
                    .NumElements = 0u,
                },
            // Specify partial primitive type, while IA determines if its a strip / list.
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1u,
            .RTVFormats =
                {
                    DXGI_FORMAT_R8G8B8A8_UNORM,
                },
            .SampleDesc = {1u, 0u},
            .NodeMask = 0u,
        };

        ComPtr<ID3D12PipelineState> pso = {};
        throw_if_failed(device->CreateGraphicsPipelineState(&graphics_pipeline_state_desc, IID_PPV_ARGS(&pso)));

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

            const std::array<f32, 4> clear_color{1.0f, 1.0f, 1.0f, 1.0f};

            graphics_command_list->ClearRenderTargetView(
                back_buffers[current_swapchain_backbuffer_index].cpu_rtv_handle, clear_color.data(), 0u, nullptr);

            ID3D12DescriptorHeap *const shader_visible_descriptor_heaps = {
                cbv_srv_uav_descriptor_heap.descriptor_heap.Get(),
            };

            graphics_command_list->SetDescriptorHeaps(1u, &shader_visible_descriptor_heaps);
            // Set pipeline state.
            graphics_command_list->SetPipelineState(pso.Get());
            graphics_command_list->SetGraphicsRootSignature(root_signature.Get());
            graphics_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            graphics_command_list->IASetIndexBuffer(&index_buffer_view);

            struct render_resources_t
            {
                u32 position_buffer_index{};
                u32 color_buffer_index{};
            };
            render_resources_t render_resources = {
                .position_buffer_index = (u32)vertex_position_buffer_creation_result.srv_index,
                .color_buffer_index = (u32)vertex_color_buffer_creation_result.srv_index,
            };
            graphics_command_list->SetGraphicsRoot32BitConstants(0u, 64u, &render_resources, 0u);

            graphics_command_list->RSSetViewports(1u, &viewport);
            graphics_command_list->RSSetScissorRects(1u, &scissor_rect);

            graphics_command_list->DrawInstanced(3u, 1u, 0u, 0u);

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

        throw_if_failed(debug_device->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY));
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
