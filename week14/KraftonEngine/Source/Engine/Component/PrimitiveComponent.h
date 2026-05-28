#pragma once

#include "Object/Reflection/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Render/Types/RenderTypes.h"
#include "Core/Types/RayTypes.h"
#include "Core/Types/CollisionTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Core/Delegate.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Proxy/DirtyFlag.h"

#include "Source/Engine/Component/PrimitiveComponent.generated.h"
class FPrimitiveSceneProxy;
class FScene;
class FMeshBuffer;
class FOctree;

// Overlap/Hit 델리게이트 시그니처
// OnComponentBeginOverlap(OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult)
DECLARE_MULTICAST_DELEGATE_SixParams(
	FComponentBeginOverlapSignature,
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* /*OtherActor*/,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/
);

// OnComponentEndOverlap(OverlappedComp, OtherActor, OtherComp, OtherBodyIndex)
DECLARE_MULTICAST_DELEGATE_FourParams(
	FComponentEndOverlapSignature,
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* /*OtherActor*/,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/
);

// OnComponentHit(HitComponent, OtherActor, OtherComp, NormalImpulse, HitResult)
DECLARE_MULTICAST_DELEGATE_FiveParams(
	FComponentHitSignature,
	UPrimitiveComponent* /*HitComponent*/,
	AActor* /*OtherActor*/,
	UPrimitiveComponent* /*OtherComp*/,
	FVector /*NormalImpulse*/,
	const FHitResult& /*HitResult*/
);

// OnComponentEndHit(HitComponent, OtherActor, OtherComp)
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FComponentEndHitSignature,
	UPrimitiveComponent* /*HitComponent*/,
	AActor* /*OtherActor*/,
	UPrimitiveComponent* /*OtherComp*/
);

UCLASS()
class UPrimitiveComponent : public USceneComponent
{
public:
	GENERATED_BODY()
	~UPrimitiveComponent() override;

	void BeginPlay() override;
	void EndPlay() override;

	void PostEditProperty(const char* PropertyName) override;
	void SetRelativeScale(const FVector& NewScale) override;

	virtual FMeshBuffer* GetMeshBuffer() const { return nullptr; }
	virtual FMeshDataView GetMeshDataView() const { return {}; }

	void SetVisibility(bool bNewVisible);
	inline bool IsVisible() const { return bIsVisible; }

	void SetCastShadow(bool bNewCastShadow);
	bool GetCastShadow() const { return bCastShadow; }

	bool GetCastShadowAsTwoSided() const { return bCastShadowAsTwoSided; }

	bool GetTranslucentSortPriority() const { return TranslucentSortPriority; }

	// 월드 공간 AABB를 FBoundingBox로 반환
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

	// --- Collision Channel / Response ---

	void SetCollisionEnabled(ECollisionEnabled InEnabled);
	ECollisionEnabled GetCollisionEnabled() const { return CollisionEnabled; }
	bool IsCollisionEnabled() const { return CollisionEnabled != ECollisionEnabled::NoCollision; }
	bool IsQueryCollisionEnabled() const;

	void SetCollisionObjectType(ECollisionChannel InChannel);
	ECollisionChannel GetCollisionObjectType() const { return ObjectType; }

	void SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse Response);
	void SetCollisionResponseToAllChannels(ECollisionResponse Response);
	ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const;
	const FCollisionResponseContainer& GetCollisionResponseContainer() const { return ResponseContainer; }

	// 두 컴포넌트 간 최소(=더 제한적인) 응답을 반환
	static ECollisionResponse GetMinResponse(const UPrimitiveComponent* A, const UPrimitiveComponent* B);

	void SetGenerateOverlapEvents(bool bInGenerateOverlapEvents);
	bool GetGenerateOverlapEvents() const { return bGenerateOverlapEvents; }

	// 서브클래스가 오버라이드할 수 있는 가상 함수 — 델리게이트 브로드캐스트 전에 호출됨
	virtual void NotifyComponentBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	virtual void NotifyComponentEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	virtual void NotifyComponentHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& HitResult);

	virtual void NotifyComponentEndHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp);

	// 멀티캐스트 델리게이트 — 외부 바인딩용
	FComponentBeginOverlapSignature OnComponentBeginOverlap;
	FComponentEndOverlapSignature OnComponentEndOverlap;
	FComponentHitSignature OnComponentHit;
	FComponentEndHitSignature OnComponentEndHit;

protected:
	void OnTransformDirty() override;
	void EnsureWorldAABBUpdated() const;

	FVector LocalExtents = { 0.5f, 0.5f, 0.5f };
	mutable FVector WorldAABBMinLocation;
	mutable FVector WorldAABBMaxLocation;
	mutable bool bWorldAABBDirty = true;
	mutable bool bHasValidWorldAABB = false;

	UPROPERTY(Edit, Save, Category="Rendering", DisplayName="Visible")
	bool bIsVisible = true;
	UPROPERTY(Edit, Save, Category="Rendering", DisplayName="Cast Shadow")
	bool bCastShadow = true;
	UPROPERTY(Edit, Save, Category="Rendering", DisplayName="Two Sided Shadow")
	bool bCastShadowAsTwoSided = false;
	UPROPERTY(Edit, Save, Category="Rendering", DisplayName="Translucent Sort Priority")
	int32 TranslucentSortPriority = 0;
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Generate Overlap Events")
	bool bGenerateOverlapEvents = false;

	// 물리 파라미터 — RootComponent의 값만 백엔드에 적용 (compound shape 정책).
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Collision Enabled", Enum=ECollisionEnabled)
	ECollisionEnabled CollisionEnabled = ECollisionEnabled::NoCollision;
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Object Type", Enum=ECollisionChannel)
	ECollisionChannel ObjectType = ECollisionChannel::WorldStatic;
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Collision Responses", Type=Struct)
	FCollisionResponseContainer ResponseContainer; // 기본: 전 채널 Block
	FPrimitiveSceneProxy* SceneProxy = nullptr;

	FOctree* OctreeNode = nullptr;
	bool bInOctreeOverflow = false;
};
