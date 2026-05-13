#pragma once

#include "Core/CoreTypes.h"

class FAssetFactory
{
public:
	static bool CreateFloatCurve(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
	static bool CreateCameraShake(const FString& DirectoryPath, const FString& AssetName, FString& OutCreatedPath);
};
