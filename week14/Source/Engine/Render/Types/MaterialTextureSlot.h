#pragma once

#include "Core/Types/CoreTypes.h"

////////////////////////////////////////////
//JSon에 Enum + Texture만 적으면 자동으로 바인딩 될겁니다. 아마도....
//t0 ~ t7은 MaterialSlot입니다.
enum class EMaterialTextureSlot : uint32
{
	Diffuse = 0,
	Normal = 1,
	Roughness = 2,
	Metallic = 3,
	Emissive = 4,
	AO = 5,
	Custom0 = 6,
	Custom1 = 7,
	Max = 8
};

namespace MaterialTextureSlot
{
	inline FString ToString(int32 SlotEnum)
	{
		switch (SlotEnum)
		{
		case (int)EMaterialTextureSlot::Diffuse:
			return FString("Diffuse");

		case (int)EMaterialTextureSlot::Normal:
			return FString("Normal");

		case (int)EMaterialTextureSlot::Roughness:
			return FString("Roughness");

		case (int)EMaterialTextureSlot::Metallic:
			return FString("Metallic");

		case (int)EMaterialTextureSlot::Emissive:
			return FString("Emissive");

		case (int)EMaterialTextureSlot::AO:
			return FString("AO");

		case (int)EMaterialTextureSlot::Custom0:
			return FString("Custom0");

		case (int)EMaterialTextureSlot::Custom1:
			return FString("Custom1");

		default:
			throw "How";
		}
	}

	inline EMaterialTextureSlot FromParameterName(const FString& ParameterName)
	{
		if (ParameterName == FString("Diffuse") || ParameterName == FString("DiffuseTexture"))
			return EMaterialTextureSlot::Diffuse;

		if (ParameterName == FString("Normal") || ParameterName == FString("NormalTexture"))
			return EMaterialTextureSlot::Normal;

		if (ParameterName == FString("Roughness") || ParameterName == FString("RoughnessTexture"))
			return EMaterialTextureSlot::Roughness;

		if (ParameterName == FString("Metallic") || ParameterName == FString("MetallicTexture"))
			return EMaterialTextureSlot::Metallic;

		if (ParameterName == FString("Emissive") || ParameterName == FString("EmissiveTexture"))
			return EMaterialTextureSlot::Emissive;

		if (ParameterName == FString("AO") || ParameterName == FString("AOTexture"))
			return EMaterialTextureSlot::AO;

		if (ParameterName == FString("Custom0") || ParameterName == FString("Custom0Texture"))
			return EMaterialTextureSlot::Custom0;

		if (ParameterName == FString("Custom1") || ParameterName == FString("Custom1Texture"))
			return EMaterialTextureSlot::Custom1;

		throw "Unknown Parameter Name";
	}
}