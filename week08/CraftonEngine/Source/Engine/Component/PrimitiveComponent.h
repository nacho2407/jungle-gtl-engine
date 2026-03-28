#pragma once

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Render/Pipeline/RenderBus.h"
#include "Render/Types/RenderTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"
#include "Render/Types/VertexTypes.h"


class UPrimitiveComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	inline const FMeshData* GetMeshData() const { return MeshData; };
	virtual FMeshBuffer* GetMeshBuffer() const { return nullptr; }

	inline void SetVisibility(bool bVisible) { bIsVisible = bVisible; }

	// 월드 공간 AABB를 FBoundingBox로 반환 (파트 B LineBatcher와의 인터페이스)
	FBoundingBox GetWorldBoundingBox() const
	{
		return FBoundingBox(WorldAABBMinLocation, WorldAABBMaxLocation);
	}

	//Collision
	virtual void UpdateWorldAABB() const;
	bool CheckAABB(const FRay& Ray);
	bool Raycast(const FRay& Ray, FHitResult& OutHitResult);
	bool IntersectTriangle(const FVector& RayOrigin, const FVector& RayDir, const FVector& V0, const FVector& V1, const FVector& V2, float& OutT);
	virtual bool RaycastMesh(const FRay& Ray, FHitResult& OutHitResult);
	inline bool IsVisible() const { return bIsVisible; }

	void UpdateWorldMatrix() const override;
	void AddWorldOffset(const FVector& WorldDelta) override;
	virtual EPrimitiveType GetPrimitiveType() const = 0;

	// MeshBuffer 기반 아웃라인 렌더링을 지원하는지 여부.
	// Batcher 처리 타입(SubUV, Text)은 false를 반환합니다.
	virtual bool SupportsOutline() const { return true; }

protected:
	const FMeshData* MeshData = nullptr;
	FVector LocalExtents = { 0.5f, 0.5f, 0.5f };
	mutable FVector WorldAABBMinLocation;
	mutable FVector WorldAABBMaxLocation;
	bool bIsVisible = true;
};

class UCubeComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UCubeComponent, UPrimitiveComponent)
	UCubeComponent();
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Cube;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};

class USphereComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(USphereComponent, UPrimitiveComponent)
	USphereComponent();
	void UpdateWorldAABB() const override;
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Sphere;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};

class UPlaneComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UPlaneComponent, UPrimitiveComponent)
	UPlaneComponent();
	void UpdateWorldAABB() const override;
	void SetRelativeScale(const FVector& NewScale);
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Plane;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};