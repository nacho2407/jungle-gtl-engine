#pragma once

#include "Core/Containers/String.h"
#include "Core/CoreTypes.h"

class FAssetPathPolicy
{
public:
	static bool FileExists(const FString& Path);
	static bool IsCurveAssetPath(const FString& Path);
	static bool IsAnimSequenceAssetPath(const FString& Path);
	static bool IsSequenceAssetPath(const FString& Path);
	static bool IsAnimStateMachineAssetPath(const FString& Path);
	static bool IsSerializedMaterialAssetPath(const FString& Path);
	static FString NormalizeAnimStateMachineAssetPath(const FString& Path);
	static bool IsSkeletalMeshSourcePath(const FString& Path);
	static bool IsStaticMeshSourcePath(const FString& Path);
	static FString MakeCookedStaticMeshBinaryPath(const FString& SourcePath);
	static FString MakeSiblingStaticMeshBinaryPath(const FString& SourcePath);
	static FString MakeStaticMeshCacheBinaryPath(const FString& SourcePath);
	static FString MakeWritableStaticMeshCacheBinaryPath(const FString& SourcePath);
	static FString MakeWritableSkeletalMeshCacheBinaryPath(const FString& SourcePath);
	static FString MakeWritableAnimSequenceCacheBinaryPath(	const FString& SourceFbxPath,const FString& TargetSkeletalMeshPath,	const FString& AnimStackName);
};
