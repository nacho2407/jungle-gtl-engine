#pragma once

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Render/Pipeline/RenderBus.h"
#include "Render/Types/RenderTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Proxy/DirtyFlag.h"

class FPrimitiveSceneProxy;
class FScene;

class UPrimitiveComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	virtual FMeshBuffer* GetMeshBuffer() const { return nullptr; }
	virtual const FMeshData* GetMeshData() const { return nullptr; }

	// Batcher 렌더 데이터 수집 (Font, SubUV 등 bBatcherRendered 전용)
	virtual void CollectRender(FRenderBus& Bus) const {}

	void SetVisibility(bool bNewVisible);
	inline bool IsVisible() const { return bIsVisible; }

	// 월드 공간 AABB를 FBoundingBox로 반환 (파트 B LineBatcher와의 인터페이스)
	FBoundingBox GetWorldBoundingBox() const;

	//Collision
	virtual void UpdateWorldAABB() const;
	virtual bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult);
	void UpdateWorldMatrix() const override;

	virtual bool SupportsOutline() const { return true; }

	// --- 렌더 상태 관리 ---
	void CreateRenderState() override;
	void DestroyRenderState() override;

	// 프록시 전체 재생성 (메시 교체 등 큰 변경 시 사용)
	void MarkRenderStateDirty();

	// 서브클래스가 오버라이드하여 자신에 맞는 구체 프록시를 생성
	virtual FPrimitiveSceneProxy* CreateSceneProxy();

	FPrimitiveSceneProxy* GetSceneProxy() const { return SceneProxy; }

	// FScene의 DirtyProxies에 등록까지 수행하는 헬퍼
	void MarkProxyDirty(EDirtyFlag Flag) const;

protected:
	FVector LocalExtents = { 0.5f, 0.5f, 0.5f };
	mutable FVector WorldAABBMinLocation;
	mutable FVector WorldAABBMaxLocation;
	bool bIsVisible = true;
	FPrimitiveSceneProxy* SceneProxy = nullptr;
};
