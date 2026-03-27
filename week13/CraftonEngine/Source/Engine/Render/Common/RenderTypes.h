#pragma once

//	Windows API Include
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>

//	D3D API Include
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>

#pragma comment(lib, "dxgi")
#include "Core/CoreTypes.h"

//	Primtive Type Enum
enum class EPrimitiveType
{
	EPT_Cube,
	EPT_Sphere,
	EPT_Plane,
	EPT_Quad,
	EPT_TransGizmo,
	EPT_RotGizmo,
	EPT_ScaleGizmo,
	EPT_Axis,
	EPT_Grid,
	EPT_Text,		// TextRenderComponent — MeshBuffer 없음, FontBatcher가 처리
	EPT_SubUV,		// SubUVComponent     — MeshBuffer 없음, SubUVBatcher가 처리
};

enum class ERenderPass : uint32
{
	Opaque,
	Font,			// TextRenderComponent → FontBatcher 경유
	SubUV,			// SubUVComponent     → SubUVBatcher 경유
	Translucent,
	StencilMask,
	Outline,
	Editor,
	Grid,
	DepthLess,
	MAX
};