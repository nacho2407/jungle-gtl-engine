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

	static void ClearCache();

	// 임시용. 추후 .obj파일 파싱으로 변경될 예정.
	static UStaticMesh* GetPrimitiveSphere();
	static UStaticMesh* GetPrimitiveSky();

private:
	static bool ParseObjFile(const FString& FilePath, FStaticMesh* OutMesh, TArray<FString>& OutMaterialNames);
};
