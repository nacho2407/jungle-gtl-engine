#pragma once

#include "Core/CoreTypes.h"
#include "Math/AABB.h"
#include "Render/Resource/VertexTypes.h"
#include "Render/Resource/Material.h"

//	Raw Data -> Cooked Static Mesh
struct ID3D11ShaderResourceView;

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

	ID3D11ShaderResourceView* DiffuseSRV = nullptr;
	ID3D11ShaderResourceView* AmbientSRV = nullptr;
	ID3D11ShaderResourceView* SpecularSRV = nullptr;
};

// Raw Data
struct FStaticMesh
{
	FString PathFileName;
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;
	TArray<FStaticMeshMaterialSlot> MaterialSlots;
	
	FAABB LocalBounds;
};