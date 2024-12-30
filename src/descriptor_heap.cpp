#include "descriptor_heap.hpp"

namespace nether
{
descriptor_heap_t::descriptor_heap_t(ID3D12Device *const device, const D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type,
                                     const u32 num_descriptors, const std::wstring_view heap_name)
{
    D3D12_DESCRIPTOR_HEAP_FLAGS descriptor_heap_flag =
        (descriptor_heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ||
         descriptor_heap_type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
            ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
            : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    const D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {
        .Type = descriptor_heap_type,
        .NumDescriptors = num_descriptors,
        .Flags = descriptor_heap_flag,
        .NodeMask = 0u,
    };

    throw_if_failed(device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap)));
    set_name_d3d12_object(descriptor_heap.Get(), heap_name.data());

    descriptor_handle_increment_size = device->GetDescriptorHandleIncrementSize(descriptor_heap_type);

    current_descriptor_handle = {
        .cpu_handle = descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
        .gpu_handle = (descriptor_heap_flag == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
                          ? descriptor_heap->GetGPUDescriptorHandleForHeapStart()
                          : D3D12_GPU_DESCRIPTOR_HANDLE{},
        .index = 0u,
    };
};

descriptor_handle_t descriptor_heap_t::get_then_offset_current_descriptor_handle()
{
    descriptor_handle_t result = current_descriptor_handle;

    current_descriptor_handle.index++;

    current_descriptor_handle.cpu_handle.ptr += descriptor_handle_increment_size;
    current_descriptor_handle.gpu_handle.ptr += descriptor_handle_increment_size;

    return result;
}

descriptor_handle_t descriptor_heap_t::get_descriptor_at_index(const u32 index) const
{
    descriptor_handle_t result = {
        .cpu_handle = descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
        .gpu_handle = descriptor_heap->GetGPUDescriptorHandleForHeapStart(),
        .index = index,
    };

    result.cpu_handle.ptr += static_cast<size_t>(descriptor_handle_increment_size * index);
    result.gpu_handle.ptr += descriptor_handle_increment_size * index;

    return result;
}

} // namespace nether
