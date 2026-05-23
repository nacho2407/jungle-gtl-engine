#pragma once

#include "Core/Types/CoreTypes.h"

class FAssetFactory
{
public:
	static bool CreateFloatCurve(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateCameraShake(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateAnimGraph(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateParticleSystem(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
};
