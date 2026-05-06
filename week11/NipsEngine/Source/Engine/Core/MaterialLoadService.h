#pragma once

#include "Core/CoreMinimal.h"

class FResourceManager;
struct ID3D11Device;

class FMaterialLoadService
{
public:
	explicit FMaterialLoadService(FResourceManager& InResourceManager);

	bool Load(const FString& MtlFilePath, const FString& ShaderName, ID3D11Device* Device);

private:
	FResourceManager& ResourceManager;
};
