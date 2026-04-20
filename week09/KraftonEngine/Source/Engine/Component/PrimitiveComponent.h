#pragma once

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/EngineTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Scene/DirtyFlag.h"

class FPrimitiveSceneProxy;
class FScene;
class FMeshBuffer;
class FOctree;
class UMaterial;

class UPrimitiveComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UPrimitiveComponent, USceneComponent)
    ~UPrimitiveComponent() override;

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;

    virtual FMeshBuffer* GetMeshBuffer() const { return nullptr; }
    virtual FMeshDataView GetMeshDataView() const { return {}; }

    void SetVisibility(bool bNewVisible);
    inline bool IsVisible() const { return bIsVisible; }

    // 월드 공간 AABB를 FBoundingBox로 반환
    FBoundingBox GetWorldBoundingBox() const;
    void MarkWorldBoundsDirty();

    // Collision
    virtual void UpdateWorldAABB() const;
    virtual bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult);
    void UpdateWorldMatrix() const override;

    virtual bool SupportsOutline() const { return true; }

    // For Material
    virtual int32 GetNumMaterials() const { return 0; }
    virtual UMaterial* GetMaterial(int32 ElementIndex) const { return nullptr; }
    virtual void SetMaterial(int32 ElementIndex, class UMaterial* InMaterial) {};


    // --- 렌더 상태 관리 ---
    void CreateRenderState() override;
    void DestroyRenderState() override;

    // 프록시 전체 재생성 (메시 교체 등 큰 변경 시 사용)
    void MarkRenderStateDirty();

    // 트랜스폼/AABB 변경 시 호출 — 프록시·Octree·PickingBVH·VisibleSet을 일괄 갱신.
    void MarkRenderTransformDirty();

    // 가시성 토글 시 호출 — 위와 동일하되 Visibility dirty 플래그를 사용.
    void MarkRenderVisibilityDirty();

    // 서브클래스가 오버라이드하여 자신에 맞는 구체 프록시를 생성
    virtual FPrimitiveSceneProxy* CreateSceneProxy();

    FPrimitiveSceneProxy* GetSceneProxy() const { return SceneProxy; }

    // FScene의 DirtyProxies에 등록까지 수행하는 헬퍼
    void MarkProxyDirty(EDirtyFlag Flag) const;

    FOctree* GetOctreeNode() const { return OctreeNode; }
    bool IsInOctreeOverflow() const { return bInOctreeOverflow; }

    void SetOctreeLocation(FOctree* InNode, bool bOverflow)
    {
        OctreeNode = InNode;
        bInOctreeOverflow = bOverflow;
    }

    void ClearOctreeLocation()
    {
        OctreeNode = nullptr;
        bInOctreeOverflow = false;
    }

protected:
    void OnTransformDirty() override;
    void EnsureWorldAABBUpdated() const;

    FVector LocalExtents = { 0.5f, 0.5f, 0.5f };
    mutable FVector WorldAABBMinLocation;
    mutable FVector WorldAABBMaxLocation;
    mutable bool bWorldAABBDirty = true;
    mutable bool bHasValidWorldAABB = false;
    bool bIsVisible = true;
    FPrimitiveSceneProxy* SceneProxy = nullptr;

    FOctree* OctreeNode = nullptr;
    bool bInOctreeOverflow = false;
};
