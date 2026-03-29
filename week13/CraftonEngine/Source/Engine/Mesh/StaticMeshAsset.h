#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/Resource/Buffer.h"
#include "Serialization/Archive.h"
#include "Engine/Object/FName.h"
#include <memory>

// Cooked Data 내부용 정점
struct FNormalVertex
{
	FVector pos;
	FVector normal;
	FVector4 color;
	FVector2 tex;
};

struct FStaticMeshSection;
struct FStaticMaterial;
struct FMaterial;

// Cooked Data — GPU용 정점/인덱스
// FStaticMeshLODResources in UE5
struct FStaticMesh
{
	std::string PathFileName;
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;

	TArray<FStaticMeshSection> Sections;

	std::unique_ptr<FMeshBuffer> RenderBuffer;

	void Serialize(FArchive& Ar)
	{
		// 1. std::string 직렬화 (문자열 길이 -> 문자열 내용)
		uint32 StringLen = static_cast<uint32>(PathFileName.size());
		Ar << StringLen;

		if (Ar.IsLoading()) PathFileName.resize(StringLen);
		if (StringLen > 0) Ar.Serialize(PathFileName.data(), StringLen);

		// 2. TArray 직렬화 (Archive.h에 만들어둔 오버로딩 덕분에 단 한 줄로 끝납니다!)
		Ar << Vertices;
		Ar << Indices;
	}
};

struct FStaticMeshSection
{
	//int32 MaterialIndex; // Index into UStaticMesh's FStaticMaterial array.
	FString MaterialSlotName;
	uint32 FirstIndex;
	uint32 NumTriangles;
};

struct FStaticMaterial
{
	std::shared_ptr<FMaterial> MaterialInterface;
	FString MaterialSlotName = "None"; // "None"은 특별한 슬롯 이름으로, OBJ 파일에서 머티리얼이 지정되지 않은 섹션에 할당됩니다.
};

struct FMaterial
{
	FString DiffuseTextureFilePath;
	FVector4 DiffuseColor;
	ID3D11ShaderResourceView* DiffuseSRV = nullptr;
};
