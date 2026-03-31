#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Engine/Object/Object.h"
#include "Render/Resource/Buffer.h"
#include "Serialization/Archive.h"
#include "Engine/Object/FName.h"
#include "Engine/Mesh/ObjManager.h"
#include "Materials/Material.h"
#include <memory>

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
	//int32 MaterialIndex; // Index into UStaticMesh's FStaticMaterial array.
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
				Mat.MaterialInterface = FObjManager::GetOrLoadMaterial(MatPath);
			}
			else
			{
				Mat.MaterialInterface = nullptr;
			}
		}

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

	void Serialize(FArchive& Ar)
	{
		Ar << PathFileName;
		Ar << Vertices;
		Ar << Indices;
		Ar << Sections;
	}
};