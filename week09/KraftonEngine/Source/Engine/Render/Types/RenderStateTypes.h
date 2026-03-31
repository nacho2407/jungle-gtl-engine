#pragma once

/*
	렌더 파이프라인 상태(DepthStencil, Blend, Rasterizer)에 사용되는 enum 정의입니다.
*/

enum class EDepthStencilState
{
	Default,
	DepthReadOnly,
	StencilWrite,
	StencilWriteOnlyEqual,
	NoDepth,

	// --- 기즈모 전용 ---
	GizmoInside,
	GizmoOutside
};

enum class EBlendState
{
	Opaque,
	AlphaBlend,
	NoColor
};

enum class ERasterizerState
{
	SolidBackCull,
	SolidFrontCull,
	SolidNoCull,
	WireFrame,
};
