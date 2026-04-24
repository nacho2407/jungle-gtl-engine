// 렌더 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resources/State/RenderStateTypes.h"

namespace RenderStateNames
{
// FEnumEntry는 렌더 처리에 필요한 데이터를 묶는 구조체입니다.
struct FEnumEntry
{
    const char* Str;
    int         Value;
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


static_assert(ARRAY_SIZE(BlendStateMap) == (int)EBlendState::MAX, "BlendStateMap must match EBlendState entries");
static_assert(ARRAY_SIZE(DepthStencilStateMap) == (int)EDepthStencilState::MAX, "DepthStencilStateMap must match EDepthStencilState entries");
static_assert(ARRAY_SIZE(RasterizerStateMap) == (int)ERasterizerState::MAX, "RasterizerStateMap must match ERasterizerState entries");

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
} // namespace RenderStateNames
