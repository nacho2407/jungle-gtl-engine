#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/Fbx/FbxImportContext.h"

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
