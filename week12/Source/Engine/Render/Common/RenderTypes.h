#pragma once

//	Windows API Include
#include <Windows.h>
#include <windowsx.h>

//	D3D API Include
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "dxguid.lib")

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_5.h>
#include "Render/Common/ComPtr.h"

#pragma comment(lib, "dxgi")
#include "Core/CoreTypes.h"

//	Primtive Type Enum
enum class EPrimitiveType
{
	EPT_TransGizmo,
	EPT_RotGizmo,
	EPT_ScaleGizmo,
	EPT_Line,
	EPT_Axis,
	EPT_Grid,
	EPT_StaticMesh,
	EPT_SkeletalMesh,
	EPT_Billboard,
	EPT_Text, // TextRenderComponent — MeshBuffer 없음, FontBatcher가 처리
	EPT_SubUV, // SubUVComponent     — MeshBuffer 없음, SubUVBatcher가 처리
	EPT_FOG,
	EPT_Decal,
	EPT_Fireball,
	EPT_Arrow,
	EPT_Shape,
	EPT_Box,
	EPT_Sphere,
	EPT_Capsule,
	EPT_ProceduralMesh,
	EPT_ParticleSystem,
	MAX
};

// Draw Command가 어떠한 pass에 속해야하는지를 표시
// NOTE: DepthPre, Shadow 등 일부 pass는 별도 RenderBus 큐에 명시적으로 수집되므로 ERenderPass로 지정할 수 없음
enum class ERenderPass : uint32
{
	Opaque,
	Decal,
	ViewModeMesh,
	DebugViewModeResolve,
	Fog,
	Sandervistan,
	FXAA,
	Font, // TextRenderComponent → FontBatcher 경유
	SubUV, // SubUVComponent     → SubUVBatcher 경유
	Translucent,
	SelectionMask,
	Grid,
	Editor,
	EditorOverlay,   // 깊이 무시 디버그 와이어 (본 등) — 항상 메시 위에 보임
	DepthLess,
	PostProcessOutline,
	MAX
};
