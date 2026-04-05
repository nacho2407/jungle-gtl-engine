#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Engine/Object/Object.h"
#include "Render/Resource/Buffer.h"
#include "Serialization/Archive.h"
#include "Engine/Object/FName.h"
#include "Engine/Mesh/ObjManager.h"
#include "Materials/Material.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include <memory>
#include <algorithm>

// Cooked Data 내부용 정점
struct FNormalVertex
{
	FVector pos;
	FVector normal;
	FVector4 color;
	FVector2 tex;
};


struct FStaticMeshSection
{
	int32 MaterialIndex = -1; // Index into UStaticMesh's FStaticMaterial array. Cached to avoid per-frame string comparison.
	FString MaterialSlotName;
	uint32 FirstIndex;
	uint32 NumTriangles;

	friend FArchive& operator<<(FArchive& Ar, FStaticMeshSection& Section)
	{
		Ar << Section.MaterialSlotName << Section.FirstIndex << Section.NumTriangles;
		return Ar;
	}
};

struct FStaticMaterial
{
	// std::shared_ptr<class UMaterialInterface> MaterialInterface;
	UMaterial* MaterialInterface;
	FString MaterialSlotName = "None"; // "None"은 특별한 슬롯 이름으로, OBJ 파일에서 머티리얼이 지정되지 않은 섹션에 할당됩니다.
	bool bIsUVScroll = false;

	friend FArchive& operator<<(FArchive& Ar, FStaticMaterial& Mat)
	{
		// 1. 슬롯 이름 직렬화 (메시 섹션과 매핑용)
		Ar << Mat.MaterialSlotName;

		// 2. 매터리얼 레퍼런스(파일 경로) 직렬화
		FString MatPath;
		if (Ar.IsSaving() && Mat.MaterialInterface)
		{
			// 현재 매터리얼의 파일 경로(.bin)를 가져옴
			MatPath = FObjManager::GetMBinaryFilePath(Mat.MaterialInterface->PathFileName);
		}

		Ar << MatPath; // 메시 bin 파일에는 문자열(경로) 1개만 저장됩니다.

		// 3. 로딩 시 해당 경로에서 매터리얼 에셋 로드
		if (Ar.IsLoading())
		{
			if (!MatPath.empty())
			{
				// 저장된 경로를 이용해 별도의 bin 파일에서 로드
				UE_LOG("Archive Loading Material: %s;", MatPath.c_str());
				Mat.MaterialInterface = FObjManager::GetOrLoadMaterial(MatPath);
			}
			else
			{
				Mat.MaterialInterface = nullptr;
			}
		}

		Ar << Mat.bIsUVScroll;

		return Ar;

		//Ar << Mat.MaterialSlotName;

		//if (Ar.IsLoading()) Mat.MaterialInterface = FObjManager::GetOrLoadMaterial(Mat.MaterialSlotName);

		//Ar << Mat.MaterialInterface->PathFileName;
		//Ar << Mat.MaterialInterface->DiffuseTextureFilePath;
		//Ar << Mat.MaterialInterface->DiffuseColor;
		//return Ar;
	}
};

// Cooked Data — GPU용 정점/인덱스
// FStaticMeshLODResources in UE5
struct FStaticMesh
{
	FString PathFileName;
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;

	TArray<FStaticMeshSection> Sections;

	std::unique_ptr<FMeshBuffer> RenderBuffer;

	// 메시 로컬 바운드 캐시 (정점 순회 1회로 계산)
	FVector BoundsCenter = FVector(0, 0, 0);
	FVector BoundsExtent = FVector(0, 0, 0);
	bool    bBoundsValid = false;

	void CacheBounds()
	{
		bBoundsValid = false;
		if (Vertices.empty()) return;

		FVector LocalMin = Vertices[0].pos;
		FVector LocalMax = Vertices[0].pos;
		for (const FNormalVertex& V : Vertices)
		{
			LocalMin.X = (std::min)(LocalMin.X, V.pos.X);
			LocalMin.Y = (std::min)(LocalMin.Y, V.pos.Y);
			LocalMin.Z = (std::min)(LocalMin.Z, V.pos.Z);
			LocalMax.X = (std::max)(LocalMax.X, V.pos.X);
			LocalMax.Y = (std::max)(LocalMax.Y, V.pos.Y);
			LocalMax.Z = (std::max)(LocalMax.Z, V.pos.Z);
		}

		BoundsCenter = (LocalMin + LocalMax) * 0.5f;
		BoundsExtent = (LocalMax - LocalMin) * 0.5f;
		bBoundsValid = true;
	}

	void Serialize(FArchive& Ar)
	{
		Ar << PathFileName;
		Ar << Vertices;
		Ar << Indices;
		Ar << Sections;
	}
};