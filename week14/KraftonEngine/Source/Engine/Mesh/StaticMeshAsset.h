#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Engine/Object/Object.h"
#include "Render/Resource/Buffer.h"
#include "Serialization/Archive.h"
#include "Engine/Object/FName.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include <memory>
#include <algorithm>
// Cooked Data 내부용 정점
struct FNormalVertex
{
	FVector pos;
	FVector normal;
	FVector4 color;
	FVector2 tex;
	FVector4 tangent;
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
	UMaterial* MaterialInterface = nullptr;
	FString MaterialSlotName = "None";	// "None"은 특별한 슬롯 이름으로, OBJ 파일에서 머티리얼이 지정되지 않은 섹션에 할당됩니다.
	FString MaterialPath;				// .mat 경로

	friend FArchive& operator<<(FArchive& Ar, FStaticMaterial& Mat)
	{
		// 1. 슬롯 이름 직렬화 (메시 섹션과 매핑용)
		Ar << Mat.MaterialSlotName;

		// 2. Material은 .bin에 직접 저장하지 않고 Asset/Materials/Auto/*.mat 경로만 저장
		if (Ar.IsSaving() && Mat.MaterialInterface)
		{
			Mat.MaterialPath = Mat.MaterialInterface->GetAssetPathFileName();
		}
		Ar << Mat.MaterialPath;

		// 3. 로딩 시 FMaterialManager를 통해 머티리얼 복원
		if (Ar.IsLoading())
		{
			if (!Mat.MaterialPath.empty())
			{
				Mat.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(Mat.MaterialPath);
			}
			else
			{
				Mat.MaterialInterface = nullptr;
			}
		}

		return Ar;
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
