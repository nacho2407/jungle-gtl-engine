#pragma once

/*
    Shader, Constant Buffer 등 렌더링에 필요한 리소스들을 관리하는 Class 입니다.
    Renderer에서 필요한 리소스들을 FRenderResources에 추가하여 관리할 수 있습니다.
*/

#include "Render/Resource/Shader.h"
#include "Render/Resource/Buffer.h"

struct FRenderResources
{
	FConstantBuffer FrameBuffer;					// b0
    FConstantBuffer PerObjectConstantBuffer;        // b1
	FConstantBuffer LightBuffer;					// b3 (Ambient, Directional Light)		

	// Compute Shader에서는 UAV로 바인딩하기 때문에 Slot이 달라질 수 있습니다.
    FStructuredBuffer DecalStructuredBuffer;        // t8 (FDecalInfo)
													// t9 DecalTextures
	FStructuredBuffer LightStructuredBuffer;		// t10 (FLightInfo)
	FStructuredBuffer LightCulledIndexBuffer;		// t11 (LightCulledIndex) (uint)
	FStructuredBuffer LightTileBuffer;				// t12 (LightTile) (uint2) 
	FStructuredBuffer MPLightStructuredBuffer;		// t13 Light (Multipass)

    FConstantBuffer LightPassConstantBuffer;		// b7
    FConstantBuffer FogPassConstantBuffer;			// b9
    FConstantBuffer FXAAConstantBuffer;				// b10
};

enum class ESamplerType
{
	EST_Point,
	EST_Linear,
	EST_Anisotropic,
};

enum class EDepthStencilType
{
	Default,
	DepthReadOnly,
	StencilWrite,
	StencilWriteOnlyEqual,

	// --- 기즈모 전용 ---
	GizmoInside,
	GizmoOutside
};

enum class EBlendType
{
	Opaque,
	AlphaBlend,
	NoColor
};

enum class ERasterizerType
{
	SolidBackCull,
	SolidFrontCull,
	SolidNoCull,
	WireFrame,
	DepthView,
};
