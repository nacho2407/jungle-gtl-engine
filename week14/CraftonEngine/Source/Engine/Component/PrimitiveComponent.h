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

	virtual FMeshBuffer* GetMeshBuffer() const { return nullptr; }
	virtual const FMeshData* GetMeshData() const { return nullptr; }

	inline void SetVisibility(bool bVisible) { bIsVisible = bVisible; }

	// 월드 공간 AABB를 FBoundingBox로 반환 (파트 B LineBatcher와의 인터페이스)
	FBoundingBox GetWorldBoundingBox() const
	{
		return FBoundingBox(WorldAABBMinLocation, WorldAABBMaxLocation);
	}

	//Collision
	virtual void UpdateWorldAABB() const;
	virtual bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult);
	inline bool IsVisible() const { return bIsVisible; }

	void UpdateWorldMatrix() const override;
	void AddWorldOffset(const FVector& WorldDelta) override;

	// Outline 셰이더에서 2D/3D 확장 방식 결정 (Plane, Billboard 등 flat 컴포넌트는 true)
	virtual bool IsFlat() const { return false; }

	// MeshBuffer 기반 아웃라인 렌더링을 지원하는지 여부.
	virtual bool SupportsOutline() const { return true; }

protected:
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

	FMeshBuffer* GetMeshBuffer() const override;
	const FMeshData* GetMeshData() const override;
};

class USphereComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(USphereComponent, UPrimitiveComponent)
	USphereComponent();
	void UpdateWorldAABB() const override;

	FMeshBuffer* GetMeshBuffer() const override;
	const FMeshData* GetMeshData() const override;
};

class UPlaneComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UPlaneComponent, UPrimitiveComponent)
	UPlaneComponent();
	void UpdateWorldAABB() const override;
	void SetRelativeScale(const FVector& NewScale);

	FMeshBuffer* GetMeshBuffer() const override;
	const FMeshData* GetMeshData() const override;
	bool IsFlat() const override { return true; }
};