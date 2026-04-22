#pragma once

#include "Core/CoreTypes.h"

/*
    Render/Execute 계층에서 공유하는 실제 실행 단계입니다.
    머티리얼이 아니라 렌더러와 파이프라인이 이 순서를 기준으로 pass 실행을 결정합니다.
*/
enum class EPipelineStep : uint8
{
    DepthPre,
    LightCulling,
    Opaque,
    Decal,
    Lighting,
    NonLitViewMode,
    PostProcess,
    Overlay,
    Present,
    Count
};

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
