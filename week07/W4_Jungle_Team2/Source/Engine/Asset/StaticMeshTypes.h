#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resource/VertexTypes.h"
#include "Render/Resource/Material.h"

//	Raw Data -> Cooked Static Mesh

class UMaterial;

struct FStaticMeshSection
{
	uint32 StartIndex = 0;
	uint32 IndexCount = 0;
	int32 MaterialSlotIndex = -1;
};

struct FStaticMeshMaterialSlot
{
	FString SlotName;
	FMaterial MaterialData;
	UMaterial* DefaultMaterial = nullptr;
};

//	CookedData
struct FStaticMesh
{
	FString PathFileName;
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;
	TArray<FStaticMeshMaterialSlot> MaterialSlots;
};