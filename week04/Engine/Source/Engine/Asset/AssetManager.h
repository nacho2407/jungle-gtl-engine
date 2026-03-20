#pragma once
#include "CoreUObject/Object.h"

class UAsset;

enum class EAssetType : uint8
{
	Unknown = 0,
	Texture,
	Mesh,
	Shader,
	Material,
};

class UAssetManager : public UObject
{
public:

};

