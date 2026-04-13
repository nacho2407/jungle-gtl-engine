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

    FConstantBuffer LightPassConstantBuffer;		// b7
    FConstantBuffer FogPassConstantBuffer;		// b9
    FConstantBuffer FXAAConstantBuffer;                     // b10

	FStructuredBuffer LightStructuredBuffer;	// t3
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
