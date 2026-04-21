#pragma once

#include "Core/CoreTypes.h"

/*
    드로우 커맨드와 상태 매니저가 공통으로 사용하는 렌더 상태 enum 모음입니다.
    실제 D3D11 상태 객체를 직접 들지 않고, 엔진 내부 식별자로만 상태를 구분합니다.
*/
enum class EDepthStencilState
{
    Default,
    DepthReadOnly,
    StencilWrite,
    StencilWriteOnlyEqual,
    NoDepth,
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
