#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include <initializer_list>

namespace MaterialSemantics
{
inline constexpr const char* DiffuseTextureSlot = "DiffuseTexture";
inline constexpr const char* NormalTextureSlot = "NormalTexture";
inline constexpr const char* SectionColorParameter = "SectionColor";
inline constexpr const char* SpecularPowerParameter = "SpecularPower";
inline constexpr const char* SpecularStrengthParameter = "SpecularStrength";

inline constexpr float DefaultSpecularPower = 32.0f;
inline constexpr float DefaultSpecularStrength = 1.0f;

inline bool MatchesAny(const FString& Name, std::initializer_list<const char*> Candidates)
{
    for (const char* Candidate : Candidates)
    {
        if (Name == Candidate)
        {
            return true;
        }
    }
    return false;
}

inline FString CanonicalizeTextureSlot(const FString& SlotName)
{
    if (MatchesAny(SlotName, { DiffuseTextureSlot, "BaseColorTexture", "AlbedoTexture", "BaseTexture", "DiffuseMap", "AlbedoMap" }))
    {
        return DiffuseTextureSlot;
    }

    if (MatchesAny(SlotName, { NormalTextureSlot, "NormalMap", "NormalMapTexture", "BumpTexture", "BumpMap" }))
    {
        return NormalTextureSlot;
    }

    return SlotName;
}

inline FString CanonicalizeParameterName(const FString& ParamName)
{
    if (MatchesAny(ParamName, { SectionColorParameter, "BaseColorTint", "DiffuseColor", "AlbedoTint", "TintColor" }))
    {
        return SectionColorParameter;
    }

    if (MatchesAny(ParamName, { SpecularPowerParameter, "Shininess", "SpecPower", "GlossPower", "Ns" }))
    {
        return SpecularPowerParameter;
    }

    if (MatchesAny(ParamName, { SpecularStrengthParameter, "SpecularIntensity", "Specular", "SpecStrength", "KsStrength" }))
    {
        return SpecularStrengthParameter;
    }

    return ParamName;
}

inline FVector4 GetDefaultSectionColor()
{
    return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
}
} // namespace MaterialSemantics
