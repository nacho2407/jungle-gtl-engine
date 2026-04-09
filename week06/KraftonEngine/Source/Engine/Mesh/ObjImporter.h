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

// 블렌더 스타일 Forward 축 선택
enum class EForwardAxis : uint8
{
	X, NegX,   // +X, -X
	Y, NegY,   // +Y, -Y
	Z, NegZ    // +Z, -Z
};

enum class EWindingOrder : uint8
{
	CCW_to_CW,  // OBJ CCW → DX CW (인덱스 [0,2,1]) — 기본값
	Keep         // 원본 유지 [0,1,2]
};

struct FImportOptions
{
	float Scale = 1.0f;
	EForwardAxis ForwardAxis = EForwardAxis::NegY;  // Blender 기본: Z-up, -Y Forward
	EWindingOrder WindingOrder = EWindingOrder::CCW_to_CW;
	static FImportOptions Default() { return {}; }
};

// OBJ/MTL 파싱 + Raw→Cooked 변환
struct FObjImporter
{
	static bool Import(const FString& ObjFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
	static bool Import(const FString& ObjFilePath, const FImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
private:
	static bool ParseObj(const FString& ObjFilePath, FObjInfo& OutObjInfo);
	static bool ParseMtl(const FString& MtlFilePath, TArray<FObjMaterialInfo>& OutMaterials);
	static bool Convert(const FObjInfo& ObjInfo, const TArray<FObjMaterialInfo>& MtlInfos, const FImportOptions& Options, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);

	static FVector RemapPosition(const FVector& ObjPos, EForwardAxis Axis);
};
