#pragma once

/*
	Constants Buffer에 사용될 구조체와 
	에 담길 RenderCommand 구조체를 정의하고 있습니다.
	RenderCommand는 Renderer에서 Draw Call을 1회 수행하기 위해 필요한 정보를 담고 있습니다.
*/

#include "Render/Common/RenderTypes.h"
#include "Render/Resource/Buffer.h"
#include "Render/Resource/Material.h"
#include "Render/Device/D3DDevice.h"
#include "Core/CoreMinimal.h"
#include "Core/ResourceTypes.h"

#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Component/PostProcess/Light/LightComponent.h"


struct ID3D11ShaderResourceView;

enum class ERenderCommandType
{
	Primitive,
	Gizmo,
	SelectionMask,
	PostProcessOutline,
	Billboard,
	DebugBox,
	DebugOBB,
	DebugDirectionalLight,
	DebugPointLight,
	DebugSpotlight,
	Grid,		// Grid 패스 — LineBatcher 경유
	Font,		// TextRenderComponent — FontBatcher 경유
	SubUV,		// SubUVComponent     — SubUVBatcher 경유
	StaticMesh,	// UStaticMeshComponent — OBJ 메시 퐁셰이딩
	Decal,
	Light,
};

enum EShadowLightType
{
	SLT_Directional,
	SLT_Point,
	SLT_Spot,
};

constexpr uint32 InvalidShadowIndex = static_cast<uint32>(-1);

//PerObject
struct FPerObjectConstants
{
	FMatrix Model;
	FMatrix WorldInvTrans;
	FVector4 Color;

	FPerObjectConstants() = default;
	FPerObjectConstants(const FMatrix& InModel, const FVector4& InColor = FVector4(1, 1, 1, 1))
		: Model(InModel), Color(InColor)
	{
		WorldInvTrans = InModel.GetInverse().GetTransposed();
	}
};

struct FFrameConstants
{
	FMatrix View;          
	FMatrix Projection;    
	FVector CameraPosition;
	float bIsOrthographic = 0.0f; // 0: Perspective, 1: Orthographic
	FVector WireframeColor;
	float bIsWireframe = 0.0f;

	FVector2 ViewportSize;
    float NearPlane;
    float FarPlane;
};

struct FAmbientLightInfo
{
	FVector Color;
	float Intensity;
};
struct FDirectionalLightInfo
{
	FVector Direction;
	float Padding0;
	FVector Color;
	float Intensity;
};
struct FLightInfo
{
	FVector Color;
	float Intensity;

	uint32 Type;
	float Radius;
	float InnerAngle;
	float OuterAngle;

	FVector Direction;
	float Falloff;

	FVector Position;
    float Padding1;
};

struct FShadowLightRequest
{
	uint32 LightIndex = InvalidShadowIndex;
    ULightComponent* LightComponent = nullptr;
    EShadowLightType Type;
    FVector WorldLocation;
    bool bCastShadows = true;
    float ShadowBias = 0.0f;
    float ShadowSlopeBias = 0.0f;
    float ShadowSharpen = 1.0f;
};

struct FShadowConstants
{
	FMatrix	VirtualViewProj;
	FMatrix DirLightViewProj;
    FVector4 ScaleOffset; // xy: Scale, zw: Offset
};

struct FShadowAtlasConstants
{
    FMatrix ShadowViewProjMatrix;	// 64

    FVector4 ScaleOffset;			// 16, xy: Scale, zw: Offset

    float ShadowBias;				// 4
    float ShadowStrength;			// 4
    float ShadowSoftness;			// 4
    uint32 ShadowType;				// 4
};

struct FUberConstants
{
	FAmbientLightInfo AmbientLight;
	FDirectionalLightInfo DirectionalLight;
	uint32 LightCount;
	uint32 DecalCount;
	float Padding[2];
};

struct FDecalInfo
{
    FMatrix InvDecalWorld;
    FVector4 ColorTint;
    uint32 TextureIndex;
    float Padding[3];
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
	float OutlineThicknessPixels = 2.0f;
	FVector2 ViewportSize = FVector2(1.0f, 1.0f);
	float Padding0 = 0.0f;
};

struct FAABBConstants
{
	FVector Min;
	float Padding0;

	FVector Max;
	float Padding1;

	FColor Color;
};

struct FOBBConstants
{
	FVector Center;
	float Padding0;
	FVector Extents;
	float Padding1;
	FMatrix Rotation; // 월드 회전 행렬 (회전만 포함, 평행 이동과 스케일 제외)
	FColor Color;
};

struct FDirectionalLightConstants
{
	FVector Position;
	FVector Direction;
	FColor Color;
};

struct FPointLightConstants
{
	FVector Position;
	float Range;
	FColor Color;
};

struct FSpotLightConstants
{
	FVector Position;
	FVector Direction;
	float InnerAngle;
	float OuterAngle;
	float Range;
	FColor Color;
};

struct FGridConstants
{
	float GridSpacing;
	int32 GridHalfLineCount;
	bool  bOrthographic;
	float Padding0[1];
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
struct FBillboardConstants
{
	UTexture* Texture = nullptr;
	float Width = 1.0f;
	float Height = 1.0f;
	FColor Color = FColor::White();
};

constexpr uint32 MaxFogLayerCount = 32;

struct FFogConstants
{
	FVector4 FogColor;
    float    FogDensity;
    float    HeightFalloff;
    float        FogHeight;
    float        FogStartDistance;
    float        FogCutoffDistance;
    float        FogMaxOpacity;
    float        Padding[2];
};

struct FFogPassConstants
{
    uint32 FogCount = 0;
    float  Padding0[3] = {0.0f, 0.0f, 0.0f};
    FFogConstants Layers[MaxFogLayerCount] = {};
};

struct FFXAAConstants
{
    float InvResolution[2]; // (1/Width, 1/Height)
    uint32 bEnabled;       // 0: off, 1: on
    float  Padding;
};

struct FLightData
{
    FVector WorldPos;
    float	Radius;
    FVector Color;
    float	Intensity;
	float	RadiusFalloff;
	float	Padding[3];
};

struct FLightPassConstants 
{
	FVector CameraWorldPos;
	uint32	LightCount;
	uint32	ViewMode;		// 4 bytes (필요시 더 압축해서 보냅니다.)
	uint32	WorldLit;		// 4 bytes
	float	Padding[2];		// 8 bytes
};

struct FRenderCommand
{
	FPerObjectConstants PerObjectConstants = {};

	//	VB, IB 모두 담고 있는 MB
	FMeshBuffer* MeshBuffer = nullptr;
	UMaterialInterface* Material = nullptr;
	uint32 SectionIndexStart = 0;
	uint32 SectionIndexCount = 0;

	FBoundingBox WorldAABB;

	union
	{
		FAABBConstants AABB;
		FOBBConstants OBB;
		FDirectionalLightConstants DirectionalLight;
		FPointLightConstants PointLight;
		FSpotLightConstants SpotLight;
		FGridConstants Grid;
		FFontConstants Font;
		FSubUVConstants SubUV;
		FBillboardConstants Billboard;  // ← 추가
        FFogConstants Fog;
        FFXAAConstants FXAA;
		FLightPassConstants Light;
		FDecalInfo Decal;
	} Constants;

	ERenderCommandType Type = ERenderCommandType::Primitive;
};
