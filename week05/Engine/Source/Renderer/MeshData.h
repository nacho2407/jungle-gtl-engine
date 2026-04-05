#pragma once
#include <d3d11.h>

#include "Renderer/RenderMesh.h"
#include "CoreMinimal.h"
#include "Scene/MeshBVH.h"
#include <memory>

struct FStaticMesh;

struct FStaticMeshLOD
{
	std::unique_ptr<FStaticMesh> Mesh = nullptr;
	float Distance = 0.0f;
	uint32 VertexCount = 0;
};

struct ENGINE_API FStaticMesh : public FRenderMesh
{
	virtual bool UpdateVertexAndIndexBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context) override;
	virtual bool CreateVertexAndIndexBuffer(ID3D11Device* Device) override;
};

struct ENGINE_API FDynamicMesh : public FRenderMesh
{
	virtual bool UpdateVertexAndIndexBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context) override;
	virtual bool CreateVertexAndIndexBuffer(ID3D11Device* Device) override;

private:
	uint32 MaxVertexCapacity = 0;
	uint32 MaxIndexCapacity = 0;
};

class ENGINE_API UStaticMesh : public UObject
{
public:
	DECLARE_RTTI(UStaticMesh, UObject)
	~UStaticMesh() override;

	FBoxSphereBounds LocalBounds;
	const FString& GetAssetPathFileName() const;

	void SetStaticMeshAsset(FStaticMesh* InStaticMesh);
	FStaticMesh* GetRenderData() const { return StaticMeshAsset; }
	int32 GetNumSections() const { return StaticMeshAsset ? StaticMeshAsset->GetNumSection() : 0; }
	bool IntersectLocalRay(const FVector& RayOrigin, const FVector& RayDirection, float& OutDistance) const;

	const TArray<std::shared_ptr<FMaterial>>& GetDefaultMaterials() const { return DefaultMaterials; }
	void AddDefaultMaterial(const std::shared_ptr<FMaterial>& InMaterial) { DefaultMaterials.push_back(InMaterial); }
	void BuildAccelerationStructureIfNeeded() const;

	FStaticMesh* GetRenderData(int32 LODIndex) const;
	FStaticMesh* GetRenderDataForDistance(float Distance) const;
	void AddLOD(std::unique_ptr<FStaticMesh> InMesh, float InDistance);
	void ClearLODs();
	uint32 GetLODCount() const;

private:
	FStaticMesh* StaticMeshAsset = nullptr;
	TArray<std::shared_ptr<FMaterial>> DefaultMaterials;
	mutable std::unique_ptr<FMeshBVH> TriangleBVH;
	TArray<FStaticMeshLOD> LODs;
};
