#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

struct FStaticMesh;

// Raw Data — OBJ 파싱 직후 상태
struct FObjInfo
{
	TArray<FVector>  Positions;       // v
	TArray<FVector2> UVs;             // vt
	TArray<FVector>  Normals;         // vn
	TArray<uint32>   PosIndices;      // f - position index
	TArray<uint32>   UVIndices;       // f - uv index
	TArray<uint32>   NormalIndices;   // f - normal index
};

// MTL 재질 정보
struct FObjMaterialInfo
{
	FString Name;            // newmtl
	FVector DiffuseColor;    // Kd
	FString DiffuseTexture;  // map_Kd
};

// OBJ/MTL 파싱 + Raw→Cooked 변환
struct FObjImporter
{
	static FObjInfo ParseObj(const std::string& FilePath);
	static TArray<FObjMaterialInfo> ParseMtl(const std::string& MtlPath);
	static FStaticMesh* Convert(const FObjInfo& ObjInfo);
};
