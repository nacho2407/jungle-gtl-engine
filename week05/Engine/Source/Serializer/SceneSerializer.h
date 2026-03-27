#pragma once
#include "../Types/String.h"
#include "EngineAPI.h"
#include <d3d11.h>
class UScene;
class ENGINE_API FSceneSerializer
{
public:

	static void Save(UScene* Scene, const FString& FilePath);
	static bool Load(UScene* Scene, const FString& FilePath, ID3D11Device* Device);
};