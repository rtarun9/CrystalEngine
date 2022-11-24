#include "Pch.hpp"

#include "ShaderCompiler.hpp"

namespace nether::ShaderCompiler
{
    // Responsible for the actual compilation of shaders.
    Comptr<IDxcCompiler3> compiler{};

    // Used to create include handle and provides interfaces for loading shader to blob, etc.
    Comptr<IDxcUtils> utils{};
    Comptr<IDxcIncludeHandler> includeHandler{};


    Shader compile(const ShaderTypes& shaderType, const std::wstring_view shaderPath)
    {
        Shader shader{};

        if (!utils)
        {
            throwIfFailed(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)));
            throwIfFailed(::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)));
            throwIfFailed(utils->CreateDefaultIncludeHandler(&includeHandler));
        }

        // Setup compilation arguments.
        const std::wstring entryPoint = [=]()
        {
            switch (shaderType)
            {
                case ShaderTypes::Vertex:
                    {
                        return L"VsMain";
                    }
                    break;

                case ShaderTypes::Pixel:
                    {
                        return L"PsMain";
                    }
                    break;

                case ShaderTypes::Compute:
                    {
                        return L"CsMain";
                    }
                    break;

                default:
                    {
                        return L"";
                    }
                    break;
            }
        }();

        const std::wstring targetProfile = [=]()
        {
            switch (shaderType)
            {
                case ShaderTypes::Vertex:
                    {
                        return L"vs_6_6";
                    }
                    break;

                case ShaderTypes::Pixel:
                    {
                        return L"ps_6_6";
                    }
                    break;

                case ShaderTypes::Compute:
                    {
                        return L"cs_6_6";
                    }
                    break;

                default:
                    {
                        return L"";
                    }
                    break;
            }
        }();

        std::vector<LPCWSTR> compilationArguments{
            L"-E",
            entryPoint.c_str(),
            L"-T",
            targetProfile.c_str(),
            DXC_ARG_PACK_MATRIX_ROW_MAJOR,
            DXC_ARG_WARNINGS_ARE_ERRORS,
            DXC_ARG_ALL_RESOURCES_BOUND,
        };

        // Indicate that the shader should be in a debuggable state if in debug mode.
        // Else, set optimization level to 03.
        if constexpr (NETHER_DEBUG_MODE)
        {
            compilationArguments.push_back(DXC_ARG_DEBUG);
        }
        else
        {
            compilationArguments.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
        }

        // Load the shader source file to a blob.
        Comptr<IDxcBlobEncoding> sourceBlob{};
        throwIfFailed(utils->LoadFile(shaderPath.data(), nullptr, &sourceBlob));

        DxcBuffer sourceBuffer{
            .Ptr = sourceBlob->GetBufferPointer(),
            .Size = sourceBlob->GetBufferSize(),
            .Encoding = 0u,
        };

        // Compile the shader.
        Microsoft::WRL::ComPtr<IDxcResult> compiledShaderBuffer{};
        const HRESULT hr = compiler->Compile(&sourceBuffer,
                                             compilationArguments.data(),
                                             static_cast<uint32_t>(compilationArguments.size()),
                                             includeHandler.Get(),
                                             IID_PPV_ARGS(&compiledShaderBuffer));
        if (FAILED(hr))
        {
            fatalError(std::wstring(L"Failed to compile shader with path : ") + shaderPath.data());
        }

        // Get compilation errors (if any).
        Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors{};
        throwIfFailed(compiledShaderBuffer->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
        if (errors && errors->GetStringLength() > 0)
        {
            const LPCSTR errorMessage = errors->GetStringPointer();
            fatalError(errorMessage);
        }

        Comptr<IDxcBlob> compiledShaderBlob{nullptr};
        compiledShaderBuffer->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiledShaderBlob), nullptr);

        shader.shaderBlob = compiledShaderBlob;
        return shader;
    }
}
