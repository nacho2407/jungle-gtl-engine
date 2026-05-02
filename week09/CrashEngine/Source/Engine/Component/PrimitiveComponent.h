// 컴포넌트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Object/ObjectFactory.h"
#include "SceneComponent.h"
#include "Render/RHI/D3D11/Common/D3D11API.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Core/Delegates/Delegate.h"
#include "Core/EngineTypes.h"
#include "Render/RHI/D3D11/Buffers/VertexTypes.h"
#include "Render/Scene/SceneProxyDirtyFlag.h"
#include "GameFramework/WorldContext.h"

class FPrimitiveProxy;
class FScene;
class FMeshBuffer;
class FOctree;
class UMaterial;
class AActor;

// UPrimitiveComponent 컴포넌트이다.
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
    void SetVisibleInEditor(bool bNewVisible);
    void SetVisibleInGame(bool bNewVisible);
    void SetEditorHelper(bool bNewHelper);

    inline bool IsVisible() const { return bIsVisible; }
    inline bool IsVisibleInEditor() const { return bVisibleInEditor; }
    inline bool IsVisibleInGame() const { return bVisibleInGame; }
    inline bool IsEditorHelper() const { return bIsEditorHelper; }

    bool ShouldRenderInWorld(EWorldType WorldType) const;
    bool ShouldRenderInCurrentWorld() const;

    /*
        현재 월드 공간 AABB를 반환합니다.
    */
    FBoundingBox GetWorldBoundingBox() const;

    /*
        월드 bounds 캐시를 다시 계산하도록 표시합니다.
    */
    void MarkWorldBoundsDirty();

    /*
        컴포넌트의 월드 AABB 캐시를 갱신합니다.
    */
    virtual void UpdateWorldAABB() const;

    /*
        컴포넌트 bounds 기준 레이 교차를 검사합니다.
    */
    virtual bool LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult);

    /*
        월드 행렬을 갱신하고 필요한 경우 bounds 캐시를 함께 이동합니다.
    */
    void UpdateWorldMatrix() const override;

    virtual bool SupportsOutline() const { return true; }

    /*
        이 컴포넌트가 사용하는 머티리얼 슬롯 수를 반환합니다.
    */
    virtual int32 GetNumMaterials() const { return 0; }

    /*
        지정한 슬롯의 머티리얼을 반환합니다.
    */
    virtual UMaterial* GetMaterial(int32 ElementIndex) const { return nullptr; }

    /*
        지정한 슬롯의 머티리얼을 교체합니다.
    */
    virtual void SetMaterial(int32 ElementIndex, class UMaterial* InMaterial) {};

    /*
        렌더 씬에 프록시를 생성하고 등록합니다.
    */
    void CreateRenderState() override;

    /*
        렌더 씬에서 프록시를 제거하고 소유 리소스를 정리합니다.
    */
    void DestroyRenderState() override;

    /*
        프록시 전체를 다시 생성해야 하는 변경을 표시합니다.
    */
    void MarkRenderStateDirty();

    /*
        transform과 bounds 변경을 렌더 씬, octree, picking BVH에 알립니다.
    */
    void MarkRenderTransformDirty();

    /*
        가시성 변경을 렌더 씬에 알립니다.
    */
    void MarkRenderVisibilityDirty();

    /*
        파생 컴포넌트가 렌더 씬에 등록할 구체 프록시를 생성합니다.
    */
    virtual FPrimitiveProxy* CreateSceneProxy();

    FPrimitiveProxy* GetSceneProxy() const { return SceneProxy; }

    /*
        프록시 변경 플래그를 표시하고 씬의 dirty queue에 등록합니다.
    */
    void MarkProxyDirty(ESceneProxyDirtyFlag Flag) const;

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

	virtual void OnComponentOverlap(UPrimitiveComponent* Other) const;
	virtual class FCollision* GetCollision() const { return nullptr; }

    DECLARE_DELEGATE(FOnComponentBeginOverlap, UPrimitiveComponent*, AActor*);
    DECLARE_DELEGATE(FOnComponentEndOverlap, UPrimitiveComponent*, AActor*);
    DECLARE_DELEGATE(FOnComponentHit, UPrimitiveComponent*, AActor*);
    
    FOnComponentBeginOverlap OnComponentBeginOverlap;
    FOnComponentEndOverlap OnComponentEndOverlap;
    FOnComponentHit OnComponentHit;

    void BroadcastComponentBeginOverlap(AActor* OtherActor);
    void BroadcastComponentEndOverlap(AActor* OtherActor);
    void BroadcastComponentHit(AActor* OtherActor);

	TArray<FOverlapInfo> GetOverlapInfos() { return OverlapInfos; }
    bool IsOverLappingActor(const AActor* Other);
    void AddOverlapInfo(UPrimitiveComponent* OtherComp);
    void RemoveOverlapInfo(UPrimitiveComponent* OtherComp);


protected:
    void OnTransformDirty() override;
    void EnsureWorldAABBUpdated() const;

    FVector LocalExtents = { 0.5f, 0.5f, 0.5f };
    mutable FVector WorldAABBMinLocation;
    mutable FVector WorldAABBMaxLocation;
    mutable bool bWorldAABBDirty = true;
    mutable bool bHasValidWorldAABB = false;
    bool bIsVisible = true;
    bool bVisibleInEditor = true;
    bool bVisibleInGame = true;
    bool bIsEditorHelper = false;
    FPrimitiveProxy* SceneProxy = nullptr;

    FOctree* OctreeNode = nullptr;
    bool bInOctreeOverflow = false;

	/**
	둘 다 False (No Collision): 구름, 먼지 파티클, 배경용 풀 조각. (충돌 연산 완전 제외)

	bGenerateOverlapEvents = True / bBlockComponent = False (Trigger): 아이템, 함정, 포탈. (통과하지만 이벤트 발생)

	bGenerateOverlapEvents = False / bBlockComponent = True (Block/Physics): 벽, 상자, 플레이어 몸통. (통과 불가, 부딪힘 판정)

	**/
	bool bGenerateOverlapEvents;	//Overlap 이벤트를 발생시킬지 여부
    bool bBlockComponent;	//물리적 차단 / Hit 이벤트 여부
    TArray<FOverlapInfo> OverlapInfos;
};

