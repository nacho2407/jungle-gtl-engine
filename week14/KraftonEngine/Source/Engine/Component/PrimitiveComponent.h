#pragma once

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Render/Types/RenderTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Proxy/DirtyFlag.h"

class FPrimitiveSceneProxy;
class FScene;
class FMeshBuffer;

struct FPrimitivePickingMetrics
{
	uint32 MeshInternalNodesVisited = 0;
	uint32 MeshLeafPacketsTested = 0;
	uint32 MeshTriangleLanesTested = 0;
	uint32 MeshTriangleMaskHits = 0;
	uint32 MeshClosestTHitUpdates = 0;
	double MeshTraversalMs = 0.0;
};

class UPrimitiveComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	virtual FMeshBuffer* GetMeshBuffer() const { return nullptr; }
	virtual const FMeshData* GetMeshData() const { return nullptr; }
	virtual const FPrimitivePickingMetrics& GetLastPickingMetrics() const { return LastPickingMetrics; }

	void SetVisibility(bool bNewVisible);
	inline bool IsVisible() const { return bIsVisible; }

	// 월드 공간 AABB를 FBoundingBox로 반환 (파트 B LineBatcher와의 인터페이스)
	FBoundingBox GetWorldBoundingBox() const;
	void MarkWorldBoundsDirty();

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
	void OnTransformDirty() override;
	void EnsureWorldAABBUpdated() const;

	FVector LocalExtents = { 0.5f, 0.5f, 0.5f };
	mutable FVector WorldAABBMinLocation;
	mutable FVector WorldAABBMaxLocation;
	mutable bool bWorldAABBDirty = true;
	bool bIsVisible = true;
	FPrimitiveSceneProxy* SceneProxy = nullptr;
	mutable FPrimitivePickingMetrics LastPickingMetrics;
};
