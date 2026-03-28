#pragma once

/*
	Constants Buffer에 사용될 구조체와 
	에 담길 RenderCommand 구조체를 정의하고 있습니다.
	RenderCommand는 Renderer에서 Draw Call을 1회 수행하기 위해 필요한 정보를 담고 있습니다.
*/

#include "Render/Types/RenderTypes.h"
#include "Render/Resource/Buffer.h"
#include "Render/Device/D3DDevice.h"
#include "Core/EngineTypes.h"
#include "Core/ResourceTypes.h"

#include "Math/Matrix.h"
#include "Math/Vector.h"

class FShader;

// HLSL Common.hlsl과 1:1 대응하는 CB 슬롯 정의
namespace ECBSlot
{
	constexpr uint32 Frame     = 0;  // b0: View/Projection/Wireframe
	constexpr uint32 PerObject = 1;  // b1: Model/Color
	constexpr uint32 Gizmo     = 2;  // b2: Gizmo state
	constexpr uint32 Editor    = 4;  // b4: Camera position
	constexpr uint32 Outline   = 5;  // b5: Outline params
}

enum class ERenderCommandType
{
	Primitive,
	Gizmo,
	StencilMask,
	SelectionOutline,
	Billboard,
	DebugBox,
	Grid,		// Grid 패스 — LineBatcher 경유
	Font,		// TextRenderComponent — FontBatcher 경유
	SubUV,		// SubUVComponent     — SubUVBatcher 경유
	StaticMesh,	// StaticMeshComponent — StaticMeshShader 사용
	MAX,
};

//PerObject
struct FPerObjectConstants
{
	FMatrix Model;
	FVector4 Color;
};

struct FFrameConstants
{
	FMatrix View;          
	FMatrix Projection;    
	float bIsWireframe;
	FVector WireframeColor;
};

struct FGizmoConstants
{
	FVector4 ColorTint;
	uint32 bIsInnerGizmo;	
	uint32 bClicking;
	uint32 SelectedAxis;
	float HoveredAxisOpacity;
};

struct FEditorConstants
{
	FVector CameraPosition; // xyz 사용, w padding
	uint32 Flag;
};

struct FOutlineConstants
{
	FVector4 OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // RGBA
	FVector OutlineInvScale;
	float OutlineOffset = 0.05f;
	uint32 PrimitiveType; // EPrimitiveType enum value
	float Padding0[3];
};

struct FAABBConstants
{
	FVector Min;
	float Padding0;

	FVector Max;
	float Padding1;

	FColor Color;
};

struct FGridConstants
{
	float GridSpacing;
	int32 GridHalfLineCount;
	float Padding0[2];
};

struct FFontConstants
{
	const FString* Text = nullptr;			// 컴포넌트 소유 문자열 참조 (프레임 내 유효)
	const FFontResource* Font = nullptr;
	float Scale = 1.0f;
};

struct FSubUVConstants
{
	const FParticleResource* Particle = nullptr;
	uint32 FrameIndex = 0;
	float Width  = 1.0f;
	float Height = 1.0f;
};

// 타입별 CB 바인딩 디스크립터 — Params 데이터를 GPU CB에 업로드
struct FConstantBufferBinding
{
	FConstantBuffer* Buffer = nullptr;	// 업데이트할 CB (nullptr이면 미사용)
	uint32 Size = 0;					// 업로드할 바이트 수
	uint32 Slot = 0;					// VS/PS CB 슬롯
};

struct FRenderCommand
{
	FMeshBuffer* MeshBuffer = nullptr;
	FShader* Shader = nullptr;
	FPerObjectConstants PerObjectConstants = {};	// b1 (공통)

	// 타입별 파라미터 (GPU CB 데이터 또는 CPU 배처 파라미터)
	union
	{
		FGizmoConstants Gizmo;
		FOutlineConstants Outline;
		FAABBConstants AABB;
		FGridConstants Grid;
		FFontConstants Font;
		FSubUVConstants SubUV;
	} Params;

	// 타입별 Extra CB 바인딩 — Params 데이터를 지정 슬롯의 CB에 업로드
	FConstantBufferBinding ExtraCB;

	EDepthStencilState DepthStencilState = static_cast<EDepthStencilState>(-1);
	EBlendState BlendState = static_cast<EBlendState>(-1);
	ERenderCommandType Type = ERenderCommandType::Primitive;
};
