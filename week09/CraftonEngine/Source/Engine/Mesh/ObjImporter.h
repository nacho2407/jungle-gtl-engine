#pragma once

#include "Core/CoreTypes.h"
#include "Engine/Object/FName.h"
#include "Math/Vector.h"

struct FStaticMesh;
struct FStaticMeshSection;

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

	// .mtl File Path
	FString MaterialLibraryFilePath;
	TArray<FStaticMeshSection> Sections;
};

// MTL 재질 정보
struct FObjMaterialInfo
{
	FName MaterialSlotName = FName::None; // newmtl
	// Kd and map_Kd are mutually exclusive
	FVector DiffuseColor; // Kd
	FString DiffuseTexture;  // map_Kd
};

// OBJ/MTL 파싱 + Raw→Cooked 변환
struct FObjImporter
{
	// Assume that only one object is defined in the .obj file
	static FObjInfo ParseObj(const FString& ObjFilePath);
	static TArray<FObjMaterialInfo> ParseMtl(const FString& MtlFilePath);
	static FStaticMesh Convert(const FObjInfo& ObjInfo);
};
