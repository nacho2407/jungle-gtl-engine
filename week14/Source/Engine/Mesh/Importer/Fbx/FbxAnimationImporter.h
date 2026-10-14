#pragma once

#include "Core/Types/CoreTypes.h"
#include "Mesh/Importer/Fbx/FbxImportContext.h"

#include <fbxsdk.h>

class FFbxAnimationImporter
{
public:
	static bool ImportAnimations(
		FbxScene*                         Scene,
		FFbxImportContext&                Context,
		const FFbxAnimationImportOptions* Options    = nullptr,
		FString*                          OutMessage = nullptr
		);
};
