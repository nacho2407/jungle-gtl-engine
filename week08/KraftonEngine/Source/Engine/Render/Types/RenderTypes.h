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
#include "Render/Types/RenderStateTypes.h"

//	Mesh Shape Enum — MeshBufferManager 조회용 (순수 기하 형상)
enum class EMeshShape
{
	Cube,
	Sphere,
	Plane,
	Quad,
	TexturedQuad,
	TransGizmo,
	RotGizmo,
	ScaleGizmo,
};

enum class ERenderPass : uint32
{
	Opaque,			// 불투명 지오메트리 (StaticMesh 등)
	Decal,			// 데칼 (DepthReadOnly)
	Lighting,		// ViewMode resolve / lighting composition
	AdditiveDecal,	// FakeLight 등
	AlphaBlend,		// 반투명 지오메트리 (Font, SubUV, Billboard, Translucent)
	SelectionMask,	// 선택 스텐실 마스크
	EditorLines,	// 디버그 라인 + 그리드 (LINELIST)
	PostProcess,	// 아웃라인 풀스크린, Fog, SceneDepth
	FXAA,			// FXAA 안티앨리어싱 (SceneColor 복사 후 실행)
	GizmoOuter,		// 기즈모 외곽 (깊이 테스트 O)
	GizmoInner,		// 기즈모 내부 (깊이 무시)
	OverlayFont,	// 스크린 공간 텍스트 (깊이 무시)
	MAX
};

inline const char* GetRenderPassName(ERenderPass Pass)
{
	static const char* Names[] = {
		"RenderPass::Opaque",
		"RenderPass::Decal",
		"RenderPass::Lighting",
		"RenderPass::AdditiveDecal",
		"RenderPass::AlphaBlend",
		"RenderPass::SelectionMask",
		"RenderPass::EditorLines",
		"RenderPass::PostProcess",
		"RenderPass::FXAA",
		"RenderPass::GizmoOuter",
		"RenderPass::GizmoInner",
		"RenderPass::OverlayFont",
	};
	static_assert(ARRAYSIZE(Names) == (uint32)ERenderPass::MAX, "Names must match ERenderPass entries");
	return Names[(uint32)Pass];
}

namespace RenderStateStrings
{
	inline constexpr FEnumEntry RenderPassMap[] =
	{
		{ "Opaque",        (int)ERenderPass::Opaque },
		{ "Decal",         (int)ERenderPass::Decal },
		{ "Lighting",      (int)ERenderPass::Lighting },
		{ "AdditiveDecal", (int)ERenderPass::AdditiveDecal },
		{ "AlphaBlend",    (int)ERenderPass::AlphaBlend },
		{ "SelectionMask", (int)ERenderPass::SelectionMask },
		{ "EditorLines",   (int)ERenderPass::EditorLines },
		{ "PostProcess",   (int)ERenderPass::PostProcess },
		{ "FXAA",          (int)ERenderPass::FXAA },
		{ "GizmoOuter",    (int)ERenderPass::GizmoOuter },
		{ "GizmoInner",    (int)ERenderPass::GizmoInner },
		{ "OverlayFont",   (int)ERenderPass::OverlayFont },
	};

	static_assert(ARRAYSIZE(RenderPassMap) == (int)ERenderPass::MAX, "RenderPassMap must match ERenderPass entries");
}
