#pragma once

#include "Core/CoreTypes.h"
#include "Engine/Geometry/AABB.h"
#include "Render/Resource/VertexTypes.h"
#include "Render/Resource/Material.h"

struct FStaticMeshSection
{
	uint32 StartIndex = 0;
	uint32 IndexCount = 0;
	int32 MaterialSlotIndex = -1;
};

struct FStaticMeshMaterialSlot
{
	FString SlotName;
	UMaterialInterface* Material = nullptr;
};

struct FStaticMeshRenderData
{
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
};

// Cooked
struct FStaticMesh
{
	FString PathFileName;
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;
	TArray<FStaticMeshMaterialSlot> Slots;
	FAABB LocalBounds;

	FStaticMeshRenderData RenderData;
};
