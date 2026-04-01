#pragma once

#include "CoreMinimal.h"
#include "Renderer/MeshData.h"

struct FModelMaterialInfo
{
	FString Name = "M_Default";
	FVector4 BaseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	FString DiffuseTexturePath;
};

enum class EObjImportAxis : uint8
{
	PosX,
	NegX,
	PosY,
	NegY,
	PosZ,
	NegZ
};

struct FObjLoadOptions
{
	bool bUseLegacyObjConversion = true;
	EObjImportAxis ForwardAxis = EObjImportAxis::PosX;
	EObjImportAxis UpAxis = EObjImportAxis::PosZ;
};

class ENGINE_API FObjManager
{
private:
	static TMap<FString, UStaticMesh*> ObjStaticMeshMap;

public:
	static UStaticMesh* LoadStaticMeshAsset(const FString& PathFileName);
	static UStaticMesh* LoadObjStaticMeshAsset(const FString& PathFileName);
	static UStaticMesh* LoadObjStaticMeshAsset(const FString& PathFileName, const FObjLoadOptions& LoadOptions);
	static UStaticMesh* LoadModelStaticMeshAsset(const FString& PathFileName);
	static bool SaveModelStaticMeshAsset(const FString& PathFileName, const FStaticMesh& StaticMesh, const TArray<FModelMaterialInfo>& MaterialInfos);
	static bool BuildModelMaterialInfosFromObj(const FString& ObjFilePath, const FString& ModelFilePath, const TArray<FString>& MaterialSlotNames, TArray<FModelMaterialInfo>& OutMaterialInfos);
	static bool ParseMtlFile(const FString& MtlFIlePath);
	static void PreloadAllObjFiles(const FString& DirecttoryPath);
	static void PreloadAllModelFiles(const FString& DirectoryPath);
	static void PreloadAllMtlFiles(const FString& DirectoryPath);

	static void ClearCache();

private:
	static bool ParseObjFile(const FString& FilePath, FStaticMesh* OutMesh, TArray<FString>& OutMaterialNames, const FObjLoadOptions& LoadOptions);
};
