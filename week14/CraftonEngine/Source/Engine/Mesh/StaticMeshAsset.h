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
// TODO: not yet implemented
struct UMaterialInterface;

// Cooked Data — GPU용 정점/인덱스
// FStaticMeshLODResources in UE5
struct FStaticMesh
{
	std::string PathFileName;
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;

	// https://github.com/EpicGames/UnrealEngine/blob/260bb2e1c5610b31c63a36206eedd289409c5f11/Engine/Source/Runtime/Engine/Public/StaticMeshResources.h#L403-L431
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
	// Index into UStaticMesh's FStaticMaterial array.
	int32 MaterialIndex; // 0, 1, 2

	// from .obj's usemtl statement
	// TODO: 실제로 UE에서는 MaterialIndex만 사용함
	FName MaterialSlotName;

	uint32 FirstIndex;
	uint32 NumTriangles;
};

struct FStaticMaterial
{
	UMaterialInterface* MaterialInterface = nullptr;

	// (Imported)MaterialSlotName이 "NAME_None"이면 MaterialInterface의 이름을 사용하도록 설정
	FName MaterialSlotName = FName::None;

	// TODO: EditorOnly
	//FName ImportedMaterialSlotName = FName::None;
};