#pragma once

#include "Core/CoreTypes.h"

class FAssetPathPolicy
{
public:
	static bool FileExists(const FString& Path);
	static bool IsCurveAssetPath(const FString& Path);
	static bool IsSerializedMaterialAssetPath(const FString& Path);
	static FString MakeCookedStaticMeshBinaryPath(const FString& SourcePath);
	static FString MakeSiblingStaticMeshBinaryPath(const FString& SourcePath);
	static FString MakeStaticMeshCacheBinaryPath(const FString& SourcePath);
	static FString MakeWritableStaticMeshCacheBinaryPath(const FString& SourcePath);
};
