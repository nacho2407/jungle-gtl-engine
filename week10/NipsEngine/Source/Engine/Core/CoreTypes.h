#pragma once

#include <DirectXMath.h>
#include <cstddef>
#include <cstdint>

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
