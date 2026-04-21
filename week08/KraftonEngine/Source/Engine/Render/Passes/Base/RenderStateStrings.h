#pragma once

#include "Core/CoreTypes.h"
#include "Render/Passes/Base/PipelineStateTypes.h"
#include "Render/Passes/Base/RenderPassTypes.h"

/*
    렌더 상태 enum을 문자열과 상호 변환할 때 사용하는 공용 테이블입니다.
    JSON, 디버그 출력, 설정 직렬화에서 같은 규칙을 쓰도록 한 곳으로 모았습니다.
*/
namespace RenderStateStrings
{
struct FEnumEntry
{
    const char* Str;
    int Value;
};

inline constexpr FEnumEntry BlendStateMap[] = {
    { "Opaque", (int)EBlendState::Opaque },
    { "AlphaBlend", (int)EBlendState::AlphaBlend },
    { "Additive", (int)EBlendState::Additive },
    { "NoColor", (int)EBlendState::NoColor },
};

inline constexpr FEnumEntry DepthStencilStateMap[] = {
    { "Default", (int)EDepthStencilState::Default },
    { "DepthReadOnly", (int)EDepthStencilState::DepthReadOnly },
    { "StencilWrite", (int)EDepthStencilState::StencilWrite },
    { "StencilWriteOnlyEqual", (int)EDepthStencilState::StencilWriteOnlyEqual },
    { "NoDepth", (int)EDepthStencilState::NoDepth },
    { "GizmoInside", (int)EDepthStencilState::GizmoInside },
    { "GizmoOutside", (int)EDepthStencilState::GizmoOutside },
};

inline constexpr FEnumEntry RasterizerStateMap[] = {
    { "SolidBackCull", (int)ERasterizerState::SolidBackCull },
    { "SolidFrontCull", (int)ERasterizerState::SolidFrontCull },
    { "SolidNoCull", (int)ERasterizerState::SolidNoCull },
    { "WireFrame", (int)ERasterizerState::WireFrame },
};

inline constexpr FEnumEntry RenderPassMap[] = {
    { "Grid", (int)ERenderPass::Grid },
    { "DepthPre", (int)ERenderPass::DepthPre },
    { "Opaque", (int)ERenderPass::Opaque },
    { "Decal", (int)ERenderPass::Decal },
    { "Lighting", (int)ERenderPass::Lighting },
    { "AdditiveDecal", (int)ERenderPass::AdditiveDecal },
    { "AlphaBlend", (int)ERenderPass::AlphaBlend },
    { "SelectionMask", (int)ERenderPass::SelectionMask },
    { "EditorLines", (int)ERenderPass::EditorLines },
    { "PostProcess", (int)ERenderPass::PostProcess },
    { "FXAA", (int)ERenderPass::FXAA },
    { "GizmoOuter", (int)ERenderPass::GizmoOuter },
    { "GizmoInner", (int)ERenderPass::GizmoInner },
    { "OverlayFont", (int)ERenderPass::OverlayFont },
};

static_assert(ARRAY_SIZE(BlendStateMap) == (int)EBlendState::MAX, "BlendStateMap must match EBlendState entries");
static_assert(ARRAY_SIZE(DepthStencilStateMap) == (int)EDepthStencilState::MAX, "DepthStencilStateMap must match EDepthStencilState entries");
static_assert(ARRAY_SIZE(RasterizerStateMap) == (int)ERasterizerState::MAX, "RasterizerStateMap must match ERasterizerState entries");
static_assert(ARRAY_SIZE(RenderPassMap) == (int)ERenderPass::MAX, "RenderPassMap must match ERenderPass entries");

template <typename EnumT, size_t N>
inline EnumT FromString(const FEnumEntry (&Map)[N], const FString& Str, EnumT Default)
{
    for (size_t i = 0; i < N; ++i)
    {
        if (Str == Map[i].Str)
        {
            return static_cast<EnumT>(Map[i].Value);
        }
    }
    return Default;
}

template <typename EnumT, size_t N>
inline const char* ToString(const FEnumEntry (&Map)[N], EnumT Value)
{
    for (size_t i = 0; i < N; ++i)
    {
        if (static_cast<EnumT>(Map[i].Value) == Value)
        {
            return Map[i].Str;
        }
    }
    return "";
}
} // namespace RenderStateStrings
