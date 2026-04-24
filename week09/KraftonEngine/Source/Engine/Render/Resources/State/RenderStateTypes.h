// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

// Logical render state identifiers used by pass state descriptions.
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

// EBlendState는 렌더 처리에서 사용할 선택지를 정의합니다.
enum class EBlendState
{
    Opaque,
    AlphaBlend,
    Additive,
    NoColor,
    MAX
};

// ERasterizerState는 렌더 처리에서 사용할 선택지를 정의합니다.
enum class ERasterizerState
{
    SolidBackCull,
    SolidFrontCull,
    SolidNoCull,
    WireFrame,
    MAX
};
