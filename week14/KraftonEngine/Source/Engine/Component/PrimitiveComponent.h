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

	// 렌더 데이터 수집 — 각 컴포넌트가 자신의 렌더 엔트리를 RenderBus에 직접 추가
	virtual void CollectRender(FRenderBus& Bus) const;
	virtual void CollectSelection(FRenderBus& Bus) const;

	void SetVisibility(bool bVisible);
	inline bool IsVisible() const { return bIsVisible; }

	// 월드 공간 AABB를 FBoundingBox로 반환 (파트 B LineBatcher와의 인터페이스)
	FBoundingBox GetWorldBoundingBox() const;
	void MarkWorldBoundsDirty();

	//Collision
	virtual void UpdateWorldAABB() const;
	virtual bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult);
	void UpdateWorldMatrix() const override;

	virtual bool SupportsOutline() const { return true; }

protected:
	void OnTransformDirty() override;
	void EnsureWorldAABBUpdated() const;

	FVector LocalExtents = { 0.5f, 0.5f, 0.5f };
	mutable FVector WorldAABBMinLocation;
	mutable FVector WorldAABBMaxLocation;
	mutable bool bWorldAABBDirty = true;
	bool bIsVisible = true;
};