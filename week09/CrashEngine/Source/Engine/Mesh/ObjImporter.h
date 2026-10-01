// 메시 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

struct FStaticMesh;
struct FStaticMeshSection;
struct FStaticMaterial;

// Raw Data — OBJ 파싱 직후 상태
struct FObjInfo
{
    TArray<FVector> Positions;    // v
    TArray<FVector2> UVs;         // vt
    TArray<FVector> Normals;      // vn
    TArray<uint32> PosIndices;    // f - position index
    TArray<uint32> UVIndices;     // f - uv index
    TArray<uint32> NormalIndices; // f - normal index

    FString ObjectName; // object name (optional)

    FString MaterialLibraryFilePath;
    TArray<FStaticMeshSection> Sections;
};

// MTL 재질 정보
struct FObjMaterialInfo
{
    FString MaterialSlotName = "None";      // newmtl
    FVector Kd = FVector(1.0f, 1.0f, 1.0f); // diffuse color
    FString map_Kd;                         // diffuse texture file path
    FString map_Bump;                       // normal texture file path
    FString map_Ks;                         // specular texture file path

    FVector Ka = FVector(0.0f, 0.0f, 0.0f); // ambient color
    FVector Ks = FVector(1.0f, 1.0f, 1.0f); // specular color
    float Ns = 32.0f;                       // specular exponent
    float Ni = 1.0f;                        // optical density
    int32 illum = 2;                        // illumination model
};

// 블렌더 스타일 Forward 축 선택
enum class EForwardAxis : uint8
{
    X,
    NegX, // +X, -X
    Y,
    NegY, // +Y, -Y
    Z,
    NegZ // +Z, -Z
};

// EWindingOrder는 메시 처리에서 사용할 선택지를 정의합니다.
enum class EWindingOrder : uint8
{
    CCW_to_CW, // OBJ CCW → DX CW (인덱스 [0,2,1]) — 기본값
    Keep       // 원본 유지 [0,1,2]
};

// FImportOptions는 메시 처리에 필요한 데이터를 묶는 구조체입니다.
struct FImportOptions
{
    float Scale = 1.0f;
    EForwardAxis ForwardAxis = EForwardAxis::NegY; // Blender 기본: Z-up, -Y Forward
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

    static FString ConvertMtlInfoToJson(const FObjMaterialInfo* MtlInfo);
    static FVector RemapPosition(const FVector& ObjPos, EForwardAxis Axis);
};
