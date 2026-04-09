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

//	Mesh Shape Enum — MeshBufferManager 조회용 (순수 기하 형상)
enum class EMeshShape
{
	Cube,
	Sphere,
	Plane,
	Quad,
	TransGizmo,
	RotGizmo,
	ScaleGizmo,
};

enum class ERenderPass : uint32
{
	Opaque,
	Font,			// TextRenderComponent → FontBatcher 경유
	SubUV,			// SubUVComponent     → SubUVBatcher 경유
	Billboard,		// BillboardComponent → BillboardBatcher 경유
	Translucent,
	SelectionMask,
	Editor,
	Grid,
	PostProcess,
	GizmoOuter,
	GizmoInner,
	OverlayFont,
	MAX
};

inline const char* GetRenderPassName(ERenderPass Pass)
{
	static const char* Names[] = {
		"RenderPass::Opaque",
		"RenderPass::Font",
		"RenderPass::SubUV",
		"RenderPass::Billboard",
		"RenderPass::Translucent",
		"RenderPass::SelectionMask",
		"RenderPass::Editor",
		"RenderPass::Grid",
		"RenderPass::PostProcess",
		"RenderPass::GizmoOuter",
		"RenderPass::GizmoInner",
		"RenderPass::OverlayFont",
	};
	static_assert(ARRAYSIZE(Names) == (uint32)ERenderPass::MAX, "Names must match ERenderPass entries");
	return Names[(uint32)Pass];
}