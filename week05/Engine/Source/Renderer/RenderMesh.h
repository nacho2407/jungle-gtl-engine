#pragma once

#include "CoreMinimal.h"
#include "StaticVertex.h"
#include "Component/PrimitiveComponent.h"
#include "Renderer/TextMeshBuilder.h"
#include "Math/BoxSphereBounds.h"
#include "MeshTopology.h"

struct ENGINE_API FRenderMesh
{
	FRenderMesh() : SortId(NextSortId++) {}
	virtual ~FRenderMesh() { Release(); }

	uint32 GetSortId() const { return SortId; }
	int32 GetNumSection() const { return static_cast<int32>(Sections.size()); }

	// 동적 매핑(Map/Unmap)을 위한 Context 필요
	virtual bool UpdateVertexAndIndexBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context) = 0;
	virtual bool CreateVertexAndIndexBuffer(ID3D11Device* Device) = 0;

	virtual void Bind(ID3D11DeviceContext* Context);
	virtual void Release();

	void UpdateLocalBound();
	float GetLocalBoundRadius() const { return LocalBoundRadius; }
	FVector GetMinCoord() const { return MinCoord; }
	FVector GetMaxCoord() const { return MaxCoord; }
	FVector GetCenterCoord() const { return (MaxCoord - MinCoord) * 0.5 + MinCoord; }

	// CPU 데이터 (공통)
	EMeshTopology Topology = EMeshTopology::EMT_Undefined;
	TArray<FVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FMeshSection> Sections;
	FString PathFileName;
	bool bIsDirty = true;

protected:
	// GPU 버퍼 (공통)
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;

	uint32 SortId = 0;
	static inline uint32 NextSortId = 0;

	FVector MinCoord = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxCoord = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	float LocalBoundRadius = 0.f;
};
