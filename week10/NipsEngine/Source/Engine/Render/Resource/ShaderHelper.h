#pragma once

#include "Core/CoreTypes.h"
#include "Core/Containers/Array.h"
#include "Render/Common/ViewTypes.h"
#include "Render/Common/ShadowTypes.h"
#include <d3d11.h>

enum class EShaderFeature : uint32
{
	None			= 0,
	HasDiffuseMap	= 1 << 0,
	HasNormalMap	= 1 << 1,
	HasSpecularMap	= 1 << 2,
	HasEmissiveMap	= 1 << 3,
	HasAlphaMask	= 1 << 4,
    CascadeVis		= 1 << 5,

	ClusterCull		= 1 << 11,
	TileCull		= 1 << 12,

	ShadowCSM		= 1 << 13,
	ShadowPSM		= 1 << 14,
	ShadowPCF		= 1 << 15,
	ShadowVSM		= 1 << 16,

};

enum class ELightingModel : uint32
{
	Unlit		= 0 << 8,
	Gouraud		= 1 << 8,
	Lambert		= 2 << 8,
	BlinnPhong	= 3 << 8,
	Heatmap		= 4 << 8,
};

enum class EVSMBlurPass : uint32
{
	Horizontal,
	Vertical,
};
enum class EShadowFilter : uint32
{
    PCF = 0,
    VSM = 1,
};

class FShaderHelper
{
public:
	static TArray<D3D_SHADER_MACRO> BuildUberLitMacros(uint32 PermutationKey);
	static TArray<D3D_SHADER_MACRO> BuildLightCullingCSMacros(ELightCullMode Mode);
	static TArray<D3D_SHADER_MACRO> BuildShadowMapMacros(EShadowMap Map);
    static TArray<D3D_SHADER_MACRO> BuildVSMBlurCSMacros(EVSMBlurPass Pass);

 };

inline EShaderFeature operator&(EShaderFeature a, EShaderFeature b)
{
	return static_cast<EShaderFeature>(static_cast<uint32>(a) & static_cast<uint32>(b));
}

inline bool operator!(EShaderFeature Feature)
{
	return Feature == EShaderFeature::None;
}
