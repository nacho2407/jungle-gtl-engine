#pragma once

#include "Core/CoreTypes.h"

class FImportedMaterialPolicy
{
public:
	static FString MakeImportedMaterialAssetName(const FString& SourceMtlPath, int32 MaterialIndex);
	static FString MakeMaterialSlotAliasKey(const FString& SourcePath, const FString& SlotName);
	static FString ResolveObjMaterialLibraryPath(const FString& ObjPath);
	static TArray<FString> CollectObjMaterialSlotNames(const FString& ObjPath);
};
