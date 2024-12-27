#pragma once

#include "common.hpp"

namespace nether
{
struct descriptor_handle_t
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{};

    // The index of the descriptor from heap start.
    u64 index{};
};

// A light weight descriptor heap abstraction that makes working with a bindless rendering approach really simple.
class descriptor_heap_t
{
  public:
    explicit descriptor_heap_t(ID3D12Device *const device, const D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type,
                               const u32 num_descriptors, const std::wstring_view heap_name);

    descriptor_handle_t get_then_offset_current_descriptor_handle();

    descriptor_handle_t get_descriptor_at_index(const u64 index) const;

  public:
    ComPtr<ID3D12DescriptorHeap> descriptor_heap{};
    descriptor_handle_t current_descriptor_handle{};

    size_t descriptor_handle_increment_size{};
};
} // namespace nether
