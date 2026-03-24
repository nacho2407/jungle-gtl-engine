#pragma once

/*
	Constants Buffer에 사용될 구조체와 
	에 담길 RenderCommand 구조체를 정의하고 있습니다.
	RenderCommand는 Renderer에서 Draw Call을 1회 수행하기 위해 필요한 정보를 담고 있습니다.
*/

#include "Render/Common/RenderTypes.h"
#include "Render/Resource/Buffer.h"
#include "Render/Device/D3DDevice.h"
#include "Core/EngineTypes.h"
#include "Core/ResourceTypes.h"

#include "Math/Matrix.h"
#include "Math/Vector.h"

enum class ERenderCommandType
{
	Primitive,
	Gizmo,
	Overlay,
	StencilMask,
	SelectionOutline,
	Billboard,
	DebugBox,
	Grid,		// Grid 패스 — LineBatcher 경유
	Font,		// TextRenderComponent — FontBatcher 경유
	SubUV,		// SubUVComponent     — SubUVBatcher 경유
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

struct FOverlayConstants
{
	FVector2 CenterScreen;
	FVector2 ViewportSize;

	float Radius;
	float Padding0[3];

	FVector4 Color;
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

struct FRenderCommand
{
	//	VB, IB 모두 담고 있는 MB
	FMeshBuffer* MeshBuffer = nullptr;
	FPerObjectConstants PerObjectConstants = {};
	FString TextData;

	union
	{
		FGizmoConstants Gizmo;
		FEditorConstants Editor;
		FOverlayConstants Overlay;
		FOutlineConstants Outline;
		FAABBConstants AABB;
		FGridConstants Grid;
	} Constants;

	// Font / SubUV 전용 데이터
	// ResourceManager가 소유하는 리소스 포인터 (참조만)
	const FTextureAtlasResource* AtlasResource = nullptr;
	uint32   FrameIndex  = 0;			// SubUV 프레임 인덱스
	FVector2 SpriteSize  = { 1.0f, 1.0f }; // Font: X = Scale / SubUV: X = Width, Y = Height

	EDepthStencilState DepthStencilState = static_cast<EDepthStencilState>(-1);
	EBlendState BlendState = static_cast<EBlendState>(-1);
	ERenderCommandType Type = ERenderCommandType::Primitive;
};
