#pragma once

// Dump of all the stuff that is common to all files throughout this project.
#ifndef UNICODE
#define UNICODE
#endif

// Windows includes.
#define WIN32_LEAN_AND_MEAN
#include <comdef.h>
#include <windows.h>
#include <wrl/client.h>

// Standard library includes.
#include <array>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// DXGI / D3D12 includes.
#include <d3d12.h>
#include <dxgi1_6.h>

// DXMath include.
#include <DirectXMath.h>

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
        _com_error err(hr);
        LPCWSTR error_message = err.ErrorMessage();

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
