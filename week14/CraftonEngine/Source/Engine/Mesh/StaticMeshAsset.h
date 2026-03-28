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

	// https://github.com/EpicGames/UnrealEngine/blob/260bb2e1c5610b31c63a36206eedd289409c5f11/Engine/Source/Runtime/Engine/Public/StaticMeshResources.h#L403-L431
	TArray<FStaticMeshSection> Sections;

	FMeshBuffer* RenderBuffer = nullptr;

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
	// Index into UStaticMesh's FStaticMaterial array.
	//int32 MaterialIndex;

	// from .obj's usemtl statement
	// TODO: 실제로 UE에서는 MaterialIndex만 사용함
	FString MaterialSlotName;

	uint32 FirstIndex;
	uint32 NumTriangles;
};

struct FStaticMaterial
{
	std::shared_ptr<FMaterial> MaterialInterface;

	// (Imported)MaterialSlotName이 "NAME_None"이면 MaterialInterface의 이름을 사용하도록 설정
	FString MaterialSlotName = "None";

	// TODO:
	// mtl 파일의 순서는 상관없이, .obj에서 사용된 순서대로 슬롯이 배치
	// NAME_None인 슬롯은 무조건 제일 마지막 슬롯으로 배치
};

struct FMaterial
{
	FString DiffuseTextureFilePath;
	FVector4 DiffuseColor;
};