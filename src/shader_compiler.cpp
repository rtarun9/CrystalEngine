#include "common.hpp"

#include "shader_compiler.hpp"

namespace nether::shader_compiler
{
static ComPtr<IDxcUtils> g_utils{};
static ComPtr<IDxcCompiler3> g_compiler{};
static ComPtr<IDxcIncludeHandler> g_include_handler{};

ComPtr<IDxcBlob> compile_shader(const std::wstring_view shader_path, const std::wstring_view target_profile,
                                const std::wstring_view entry_point)
{
    ComPtr<IDxcBlob> result = {};

    if (!g_utils)
    {
        throw_if_failed(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&g_utils)));
        throw_if_failed(::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&g_compiler)));
        throw_if_failed(g_utils->CreateDefaultIncludeHandler(&g_include_handler));
    }

    // Setup compilation arguments.
    std::vector<LPCWSTR> compilation_arguments = {
        L"-HV",
        L"2021",
        L"-E",
        entry_point.data(),
        L"-T",
        target_profile.data(),
        DXC_ARG_PACK_MATRIX_ROW_MAJOR,
        DXC_ARG_WARNINGS_ARE_ERRORS,
        DXC_ARG_ALL_RESOURCES_BOUND,
    };

    // Indicate that the shader should be in a debuggable state if in debug mode.
    // Else, set optimization level to 03.
    if constexpr (NETHER_DEBUG)
    {
        compilation_arguments.push_back(DXC_ARG_DEBUG);
    }
    else
    {
        compilation_arguments.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
    }

    // Load the shader source file to a blob.
    ComPtr<IDxcBlobEncoding> source_blob{nullptr};
    throw_if_failed(g_utils->LoadFile(shader_path.data(), nullptr, &source_blob));

    const DxcBuffer source_buffer = {
        .Ptr = source_blob->GetBufferPointer(),
        .Size = source_blob->GetBufferSize(),
        .Encoding = 0u,
    };

    // Compile the shader.
    ComPtr<IDxcResult> compiled_shader_buffer{};
    const HRESULT hr = g_compiler->Compile(&source_buffer, compilation_arguments.data(),
                                           static_cast<uint32_t>(compilation_arguments.size()), g_include_handler.Get(),
                                           IID_PPV_ARGS(&compiled_shader_buffer));
    if (FAILED(hr))
    {
        std::wcout << "Failed to compile shader with path : " << shader_path;
    }

    // Get compilation errors (if any).
    ComPtr<IDxcBlobUtf8> errors{};
    throw_if_failed(compiled_shader_buffer->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
    if (errors && errors->GetStringLength() > 0)
    {
        const LPCSTR error_message = errors->GetStringPointer();
        std::wcout << "Shader compiler error message : " << error_message;
    }

    ComPtr<IDxcBlob> compiled_shader_blob{nullptr};
    compiled_shader_buffer->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiled_shader_blob), nullptr);

    return compiled_shader_blob;
}
} // namespace nether::shader_compiler
