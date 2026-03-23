#pragma once

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Common/RenderTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"

struct FMeshData;


class UPrimitiveComponent : public USceneComponent
{
protected:
	const FMeshData* MeshData = nullptr;
	FVector LocalExtents = { 0.5f, 0.5f, 0.5f };
	mutable FVector WorldAABBMinLocation;
	mutable FVector WorldAABBMaxLocation;
	bool bIsVisible = true;

public:
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	inline const FMeshData* GetMeshData() const { return MeshData; };

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
};

class UCubeComponent : public UPrimitiveComponent
{
private:

public:
	DECLARE_CLASS(UCubeComponent, UPrimitiveComponent)
	UCubeComponent();
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Cube;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};

class USphereComponent : public UPrimitiveComponent
{
private:

public:
	DECLARE_CLASS(USphereComponent, UPrimitiveComponent)
	USphereComponent();
	void UpdateWorldAABB() const override;
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Sphere;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};

class UPlaneComponent : public UPrimitiveComponent
{
private:

public:
	DECLARE_CLASS(UPlaneComponent, UPrimitiveComponent)
	UPlaneComponent();
	void UpdateWorldAABB() const override;
	void SetRelativeScale(const FVector& NewScale);
	static constexpr EPrimitiveType PrimitiveType = EPrimitiveType::EPT_Plane;

	EPrimitiveType GetPrimitiveType() const override { return PrimitiveType; }
};