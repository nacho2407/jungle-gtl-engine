#pragma once

#include "Core/Types/CoreTypes.h"
#include "Mesh/Importer/Fbx/FbxImportContext.h"

#include <fbxsdk.h>

class FFbxSkinWeightImporter
{
public:
	static bool ImportSkin(FbxScene* Scene, FFbxImportContext& Context, FString* OutMessage = nullptr);
};
