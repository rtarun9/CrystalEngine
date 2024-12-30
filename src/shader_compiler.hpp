#pragma once

#include <dxcapi.h>

namespace nether::shader_compiler
{
// Helper function to compiler shaders using DXC's api.
ComPtr<IDxcBlob> compile_shader(const std::wstring_view shader_path, const std::wstring_view target_profile,
                                const std::wstring_view entry_point);
} // namespace nether::shader_compiler
