#pragma once

#include <DirectXMath.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

using int8 = __int8;
using int16 = __int16;
using int32 = __int32;
using int64 = __int64;

using uint8 = unsigned __int8;
using uint16 = unsigned __int16;
using uint32 = unsigned __int32;
using uint64 = unsigned __int64;

using Float2 = DirectX::XMFLOAT2;
using Float3 = DirectX::XMFLOAT3;
using Float4 = DirectX::XMFLOAT4;

using XMVector = DirectX::XMVECTOR;
using FXMVector = DirectX::FXMVECTOR;
using GXMVector = DirectX::GXMVECTOR;
using HXMVector = DirectX::HXMVECTOR;
using CXMVector = DirectX::CXMVECTOR;

using Float4X4 = DirectX::XMFLOAT4X4;

using XMMatrix = DirectX::XMMATRIX;
using FXMMatrix = DirectX::FXMMATRIX;
using CXMMatrix = DirectX::CXMMATRIX;

using SIZE_T = std::size_t;

using ANSICHAR = char;
using WIDECHAR = wchar_t;
using TCHAR = WIDECHAR;
inline uint32 GetTypeHash(uint32 Value)
{
    return Value;
}

inline uint32 GetTypeHash(int32 Value)
{
    return static_cast<uint32>(Value);
}

inline uint32 GetTypeHash(uint64 Value)
{
    return static_cast<uint32>(Value ^ (Value >> 32));
}

inline uint32 GetTypeHash(int64 Value)
{
    return GetTypeHash(static_cast<uint64>(Value));
}

template <typename T>
inline uint32 GetTypeHash(T* Ptr)
{
    const uintptr_t Value = reinterpret_cast<uintptr_t>(Ptr);
    return static_cast<uint32>(Value ^ (Value >> 32));
}

template <typename T>
inline std::enable_if_t<std::is_enum_v<T>, uint32> GetTypeHash(T Value)
{
    return static_cast<uint32>(Value);
}
