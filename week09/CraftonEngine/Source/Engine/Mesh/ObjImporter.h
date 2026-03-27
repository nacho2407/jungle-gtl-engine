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

	FString ObjectName; // object name (optional)

	// .mtl File Path (relative to .obj)
	// Assume that single material library is used
	FString MaterialLibrary;

	struct FObjMaterialSection
	{
		FString MaterialName; // usemtl MaterialName
		uint32 StartIndex; // Indices[StartIndex]부터 이 Material이 적용됨
		uint32 IndexCount; // 이 Material이 적용되는 인덱스 수
	};
	TArray<FObjMaterialSection> MaterialSections;
};

// MTL 재질 정보
struct FObjMaterialInfo
{
	FString Name; // newmtl
	// Kd and map_Kd are mutually exclusive
	FVector DiffuseColor; // Kd
	FString DiffuseTexture;  // map_Kd
};

// OBJ/MTL 파싱 + Raw→Cooked 변환
struct FObjImporter
{
	// Assume that only one object is defined in the .obj file
	static FObjInfo ParseObj(const std::string& FilePath);
	static TArray<FObjMaterialInfo> ParseMtl(const std::string& MtlPath);
	static FStaticMesh Convert(const FObjInfo& ObjInfo);
};
