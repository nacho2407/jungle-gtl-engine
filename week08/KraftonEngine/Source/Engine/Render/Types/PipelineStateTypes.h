    #pragma once

    #include "Core/CoreTypes.h"

    /*
        드로우 커맨드와 D3D11 상태 매니저가 공유하는 렌더 상태 enum 모음입니다.
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
	GizmoOutside,
	MAX
};

enum class EBlendState
{
	Opaque,
	AlphaBlend,
	Additive,
	NoColor,
	MAX
};

enum class ERasterizerState
{
	SolidBackCull,
	SolidFrontCull,
	SolidNoCull,
	WireFrame,
	MAX
};
