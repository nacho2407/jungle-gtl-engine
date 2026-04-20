#pragma once

#include "Core/CoreMinimal.h"

enum class EShadingModel : uint8
{
    Unlit = 0,
    Lambert,
    Phong,
    Gouraud,
    Count
};

namespace ShaderPermutationBits
{
    constexpr uint32 ShadingModelMask = 0x7u;
    constexpr uint32 NormalMapBit = 1u << 3;
}

struct FShaderPermutationDesc
{
    EShadingModel ShadingModel = EShadingModel::Unlit;
    bool bUseNormalMap = false;

    constexpr uint32 ToKey() const
    {
        uint32 Key = static_cast<uint32>(ShadingModel) & ShaderPermutationBits::ShadingModelMask;
        Key |= bUseNormalMap ? ShaderPermutationBits::NormalMapBit : 0u;
        return Key;
    }

    static constexpr FShaderPermutationDesc FromKey(uint32 Key)
    {
        FShaderPermutationDesc Desc = {};
        Desc.ShadingModel =
            static_cast<EShadingModel>(Key & ShaderPermutationBits::ShadingModelMask);
        Desc.bUseNormalMap = (Key & ShaderPermutationBits::NormalMapBit) != 0;
        return Desc;
    }

    constexpr bool operator==(const FShaderPermutationDesc& Other) const
    {
        return ToKey() == Other.ToKey();
    }
};
