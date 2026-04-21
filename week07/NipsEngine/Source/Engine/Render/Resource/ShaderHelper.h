#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/Array.h"
#include <d3d11.h>

enum class EShaderFeature : uint32
{
	None			= 0,
	HasDiffuseMap	= 1 << 0,
	HasNormalMap	= 1 << 1,
	HasSpecularMap	= 1 << 2,
	HasEmissiveMap	= 1 << 3,
	HasAlphaMask	= 1 << 4,
};

enum class ELightingModel : uint32
{
	Unlit		= 0 << 8,
	Gouraud		= 1 << 8,
	Lambert		= 2 << 8,
	BlinnPhong	= 3 << 8,
	Heatmap		= 4 << 8,
};

class FShaderHelper
{
public:
	static TArray<D3D_SHADER_MACRO> BuildUberLitMacros(uint32 PermutationKey);
};

inline EShaderFeature operator&(EShaderFeature a, EShaderFeature b)
{
	return static_cast<EShaderFeature>(static_cast<uint32>(a) & static_cast<uint32>(b));
}

inline bool operator!(EShaderFeature Feature)
{
	return Feature == EShaderFeature::None;
}
