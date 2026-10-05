#pragma once

#include "Core/CoreMinimal.h"

class FResourceManager;
class UMaterial;
class UMaterialInstance;

class FMaterialSerializationService
{
public:
	explicit FMaterialSerializationService(FResourceManager& InResourceManager);

	bool SerializeMaterial(const FString& MatFilePath, const UMaterial* Material);
	bool SerializeMaterialInstance(const FString& MatInstFilePath, const UMaterialInstance* MaterialInstance);
	bool DeserializeMaterial(const FString& MatFilePath);

private:
	FResourceManager& ResourceManager;
};
