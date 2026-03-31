#pragma once

#include "CoreMinimal.h"
#include "Renderer/MeshData.h"

class ENGINE_API FObjManager
{
private:
	static TMap<FString, UStaticMesh*> ObjStaticMeshMap;

public:
	static UStaticMesh* LoadObjStaticMeshAsset(const FString& PathFileName);
	static bool ParseMtlFile(const FString& MtlFIlePath);
	static void PreloadAllObjFiles(const FString& DirecttoryPath);

	static void ClearCache();

private:
	static bool ParseObjFile(const FString& FilePath, FStaticMesh* OutMesh, TArray<FString>& OutMaterialNames);
};
