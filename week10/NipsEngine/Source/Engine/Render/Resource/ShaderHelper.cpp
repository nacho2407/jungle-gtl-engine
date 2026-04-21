#include "ShaderHelper.h"

TArray<D3D_SHADER_MACRO> FShaderHelper::BuildUberLitMacros(uint32 PermutationKey)
{
	constexpr uint32 FeatureMask = 0xFF;
	constexpr uint32 LightingMask = 0x700;

	EShaderFeature Features =
		static_cast<EShaderFeature>(PermutationKey & FeatureMask);

	ELightingModel LightingModel =
		static_cast<ELightingModel>((PermutationKey & LightingMask));

	TArray<D3D_SHADER_MACRO> Macros;

	auto AddMacro = [&](const char* Name, const char* Definition) {
		Macros.push_back({ Name, Definition });
	};

	int MacroCount = 0;

	if (!!(Features & EShaderFeature::HasDiffuseMap))  AddMacro("HAS_DIFFUSE_MAP", "1");
	if (!!(Features & EShaderFeature::HasNormalMap))   AddMacro("HAS_NORMAL_MAP", "1");
	if (!!(Features & EShaderFeature::HasSpecularMap)) AddMacro("HAS_SPECULAR_MAP", "1");
	if (!!(Features & EShaderFeature::HasEmissiveMap)) AddMacro("HAS_EMISSIVE_MAP", "1");
	if (!!(Features & EShaderFeature::HasAlphaMask))   AddMacro("HAS_ALPHA_MASK", "1");

	switch (LightingModel)
	{
	case ELightingModel::Unlit:      AddMacro("LIGHTING_MODEL_UNLIT", "1"); break;
	case ELightingModel::Gouraud:    AddMacro("LIGHTING_MODEL_GOURAUD", "1"); break;
	case ELightingModel::Lambert:    AddMacro("LIGHTING_MODEL_LAMBERT", "1"); break;
	case ELightingModel::Heatmap:	 AddMacro("LIGHT_HEATMAP", "1"); break;
	case ELightingModel::BlinnPhong:
	default:                         AddMacro("LIGHTING_MODEL_PHONG", "1"); break;
	}

	Macros.push_back({ nullptr, nullptr });
	return Macros;
}