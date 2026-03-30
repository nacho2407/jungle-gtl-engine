#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

struct FStaticMesh;
struct FStaticMeshSection;
struct FStaticMaterial;

// Raw Data — OBJ 파싱 직후 상태
struct FObjInfo
{
	TArray<FVector>  Positions;       // v
	TArray<FVector2> UVs;             // vt
	TArray<FVector>  Normals;         // vn
	TArray<uint32>   PosIndices;      // f - position index
	TArray<uint32>   UVIndices;       // f - uv index
	TArray<uint32>   NormalIndices;   // f - normal index

	FString ObjectName; // object name (optional)

	FString MaterialLibraryFilePath;
	TArray<FStaticMeshSection> Sections;
};

// MTL 재질 정보
struct FObjMaterialInfo
{
	FString MaterialSlotName = "None"; // newmtl
	FVector Kd; // diffuse color
	FString map_Kd; // diffuse texture file path

	FVector Ka; // ambient color
	FVector Ks; // specular color
	float Ns; // specular exponent
	float Ni; // optical density
	int32 illum; // illumination model
};

// OBJ/MTL 파싱 + Raw→Cooked 변환
struct FObjImporter
{
	static bool Import(const FString& ObjFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
private:
	static bool ParseObj(const FString& ObjFilePath, FObjInfo& OutObjInfo);
	static bool ParseMtl(const FString& MtlFilePath, TArray<FObjMaterialInfo>& OutMaterials);
	static bool Convert(const FObjInfo& ObjInfo, const TArray<FObjMaterialInfo>& MtlInfos, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
};
