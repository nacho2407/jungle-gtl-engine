#pragma once

#pragma once

/*
	Constants Buffer에 사용될 구조체와 
	에 담길 RenderCommand 구조체를 정의하고 있습니다.
	RenderCommand는 Renderer에서 Draw Call을 1회 수행하기 위해 필요한 정보를 담고 있습니다.
*/

#include "Render/Common/RenderTypes.h"
#include "Render/Resource/Buffer.h"
#include "Render/Device/D3DDevice.h"

#include "Math/Matrix.h"
#include "Math/Vector.h"

enum class ERenderCommandType
{
	Primitive,
	Gizmo,
	Overlay,
	Axis,
	Grid,
	SelectionOutline
};

//	Object를 위한 Constant Buffer입니다.
struct FTransformConstants
{
	FMatrix Model;
	FMatrix View;
	FMatrix Projection;
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
	FVector4 CameraPosition; // xyz 사용, w padding
	uint32 Flag;
	float Padding0[3];
};

struct FOutlineConstants
{
	FVector4 OutlineColor = FVector4(1.0f, 0.5f, 0.0f, 1.0f); // RGBA
	FVector OutlineInvScale;
	float OutlineOffset = 0.05f;
	uint32 PrimitiveType; // EPrimitiveType enum value
	float Padding0[3];
};

// [B] Line Batcher Constants 
struct FLineConstants {
	FVector4 Color;
	float Thickness;
	float Padding[3];
};

// [C] Billboard Constants
struct FBillboardConstants {
	FVector4 ColorTint;
	uint32 BillboardType; // 0: Spherical, 1: Cylindrical
	float Opacity;
	float Padding[2];
};

struct FRenderCommand
{
	//	VB, IB 모두 담고 있는 MB
	FMeshBuffer* MeshBuffer = nullptr;
	FTransformConstants TransformConstants = {};

	FGizmoConstants GizmoConstants;
	FEditorConstants EditorConstants;
	FOverlayConstants OverlayConstants;
	FOutlineConstants OutlineConstants ;

	EDepthStencilState DepthStencilState = EDepthStencilState::Default;
	EBlendState BlendState = EBlendState::Opaque;
	ERenderCommandType Type = ERenderCommandType::Primitive;
};