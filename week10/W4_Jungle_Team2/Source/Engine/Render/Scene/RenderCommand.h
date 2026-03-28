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


struct ID3D11ShaderResourceView;

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
	StaticMesh,	// UStaticMeshComponent — OBJ 메시 퐁셰이딩
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

// StaticMeshBuffer (b6) — ShaderStaticMesh.hlsl 대응
// 완전 Obj전용입니다. 추후 Bump를 Normal로 바꾸면 됩니다.
struct FStaticMeshConstants
{
	// Phong Material
	FVector AmbientColor = { 0.2f, 0.2f, 0.2f };
	float   Padding0 = 0.0f;
	FVector DiffuseColor = { 0.8f, 0.8f, 0.8f };
	float   Padding1 = 0.0f;
	FVector SpecularColor = { 0.5f, 0.5f, 0.5f };
	float   Shininess = 32.0f;

	// Camera
	FVector CameraWorldPos = { 0.0f, 0.0f, 0.0f };
	float   Padding2 = 0.0f;

	// Texture
	ID3D11ShaderResourceView* DiffuseSRV  = { nullptr };
	bool bHasDiffuseMap = { false };
	ID3D11ShaderResourceView* AmbientSRV  = { nullptr };
	bool bHasAmbientMap = { false };
	ID3D11ShaderResourceView* SpecularSRV = { nullptr };
	bool bHasSpecularMap = { false };
	ID3D11ShaderResourceView* BumpSRV = { nullptr };
	bool bHasBumpMap = { false };
};

struct FRenderCommand
{
	//	VB, IB 모두 담고 있는 MB
	FMeshBuffer* MeshBuffer = nullptr;
	uint32		 SectionIndexStart = {};
	uint32		 SectionIndexCount = {};

	FPerObjectConstants PerObjectConstants = {};

	union
	{
		FGizmoConstants Gizmo;
		FEditorConstants Editor;
		FOutlineConstants Outline;
		FAABBConstants AABB;
		FGridConstants Grid;
		FFontConstants Font;
		FSubUVConstants SubUV;
		FStaticMeshConstants StaticMesh;
	} Constants;

	EDepthStencilState DepthStencilState = static_cast<EDepthStencilState>(-1);
	EBlendState BlendState = static_cast<EBlendState>(-1);
	ERenderCommandType Type = ERenderCommandType::Primitive;
};
