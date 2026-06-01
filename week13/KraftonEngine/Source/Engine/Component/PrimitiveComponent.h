#pragma once

#include "Object/Reflection/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Render/Types/RenderTypes.h"
#include "Core/Types/RayTypes.h"
#include "Core/Types/CollisionTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Core/Delegate.h"
#include "Physics/BodyInstance.h"
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

	UPrimitiveComponent();
	~UPrimitiveComponent() override;

	void BeginPlay() override;
	void EndPlay() override;
	void RouteComponentDestroyed() override;
    void BeginDestroy() override;

	void PostEditProperty(const char* PropertyName) override;

	virtual FMeshBuffer* GetMeshBuffer() const { return nullptr; }
	virtual FMeshDataView GetMeshDataView() const { return {}; }

	UFUNCTION(Callable, Exec, Category="Rendering")
	void SetVisibility(bool bNewVisible);
	UFUNCTION(Pure, Category="Rendering")
	inline bool IsVisible() const { return bIsVisible; }

	UFUNCTION(Callable, Category="Rendering")
	void SetCastShadow(bool bNewCastShadow);
	UFUNCTION(Pure, Category="Rendering")
	bool GetCastShadow() const { return bCastShadow; }

	UFUNCTION(Pure, Category="Rendering")
	bool GetCastShadowAsTwoSided() const { return bCastShadowAsTwoSided; }

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

	UFUNCTION(Callable, Exec, Category="Collision")
	void SetCollisionEnabled(ECollisionEnabled InEnabled);
	UFUNCTION(Pure, Category="Collision")
	ECollisionEnabled GetCollisionEnabled() const { return CollisionEnabled; }
	UFUNCTION(Pure, Category="Collision")
	bool IsCollisionEnabled() const { return CollisionEnabled != ECollisionEnabled::NoCollision; }
	UFUNCTION(Pure, Category="Collision")
	bool IsQueryCollisionEnabled() const;
	UFUNCTION(Pure, Category = "Collision")
	bool IsPhysicsCollisionEnabled() const;

	UFUNCTION(Callable, Exec, Category="Collision")
	void SetCollisionObjectType(ECollisionChannel InChannel);
	UFUNCTION(Pure, Category="Collision")
	ECollisionChannel GetCollisionObjectType() const { return ObjectType; }

	UFUNCTION(Callable, Exec, Category="Collision")
	void SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse Response);
	UFUNCTION(Callable, Exec, Category="Collision")
	void SetCollisionResponseToAllChannels(ECollisionResponse Response);
	UFUNCTION(Pure, Category="Collision")
	ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const;
	const FCollisionResponseContainer& GetCollisionResponseContainer() const { return ResponseContainer; }

	// 두 컴포넌트 간 최소(=더 제한적인) 응답을 반환
	static ECollisionResponse GetMinResponse(const UPrimitiveComponent* A, const UPrimitiveComponent* B);

	// --- Overlap / Hit ---

	UFUNCTION(Callable, Exec, Category="Physics")
	void SetSimulatePhysics(bool bInSimulate);
	UFUNCTION(Pure, Category="Physics")
	bool GetSimulatePhysics() const { return bSimulatePhysics; }
	UFUNCTION(Callable, Exec, Category="Physics")
	void SetKinematicPhysics(bool bInKinematic);
	UFUNCTION(Pure, Category="Physics")
	bool GetKinematicPhysics() const { return bKinematicPhysics; }

	// --- BodyInstance
	FBodyInstance& GetBodyInstance() { return BodyInstance; }
	const FBodyInstance& GetBodyInstance() const { return BodyInstance; }

	// --- Physics Force/Velocity API ---
	UFUNCTION(Callable, Category="Physics")
	void AddForce(const FVector& Force);
	UFUNCTION(Callable, Category="Physics")
	void AddImpulse(const FVector& Impulse);
	UFUNCTION(Callable, Category="Physics")
	void AddForceAtLocation(const FVector& Force, const FVector& Location);
	UFUNCTION(Callable, Category="Physics")
	void AddTorque(const FVector& Torque);
	UFUNCTION(Pure, Category="Physics")
	FVector GetLinearVelocity() const;
	UFUNCTION(Callable, Category="Physics")
	void SetLinearVelocity(const FVector& Vel);
	UFUNCTION(Pure, Category="Physics")
	FVector GetAngularVelocity() const;
	UFUNCTION(Callable, Category="Physics")
	void SetAngularVelocity(const FVector& Vel);

	// --- Mass / Center of Mass ---
	// Compound shape에선 RootComponent의 값만 백엔드에 적용된다.
	// 자식 컴포넌트의 Mass / CenterOfMassOffset은 직렬화는 되지만 무시.
	UFUNCTION(Callable, Exec, Category="Physics")
	void SetMass(float NewMass);
	UFUNCTION(Pure, Category="Physics")
	float GetMass() const;
	UFUNCTION(Callable, Category="Physics")
	void SetCenterOfMass(const FVector& LocalOffset);
	UFUNCTION(Pure, Category="Physics")
	FVector GetCenterOfMass() const;

	UFUNCTION(Callable, Exec, Category="Collision")
	void SetGenerateOverlapEvents(bool bInGenerateOverlapEvents);
	UFUNCTION(Pure, Category="Collision")
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

	void InitializeBodyInstance();
	// 컴포넌트가 BeginPlay 후에만 PhysicsScene::RebuildBody 호출. 이전이면 skip.
	void NotifyPhysicsBodyDirty();

	FVector LocalExtents = { 0.5f, 0.5f, 0.5f };
	mutable FVector WorldAABBMinLocation;
	mutable FVector WorldAABBMaxLocation;
	mutable bool bWorldAABBDirty = true;
	mutable bool bHasValidWorldAABB = false;

	FVector CachedPhysicsWorldScale = FVector::OneVector;
	bool bHasCachedPhysicsWorldScale = false;
	// PrimitiveComponent::BeginPlay에서 PhysicsScene::RegisterComponent를 호출한 직후 true가 된다.
	// setter들이 이 플래그를 보고 PhysicsScene 측 RebuildBody를 호출할지 결정한다.
	// (BeginPlay 전 InitDefaultComponents 단계에서 setter가 호출돼도 PhysicsScene 호출은 skip되어
	//  멤버만 변경 → BeginPlay에서 한 번 정확한 값으로 등록됨.)
	bool bComponentHasBegunPlay = false;
	UPROPERTY(Edit, Save, Category="Rendering", DisplayName="Visible")
	bool bIsVisible = true;
	UPROPERTY(Edit, Save, Category="Rendering", DisplayName="Cast Shadow")
	bool bCastShadow = true;
	UPROPERTY(Edit, Save, Category="Rendering", DisplayName="Two Sided Shadow")
	bool bCastShadowAsTwoSided = false;
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Simulate Physics")
	bool bSimulatePhysics = false;
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Kinematic Physics")
	bool bKinematicPhysics = false;
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Generate Overlap Events")
	bool bGenerateOverlapEvents = false;

	// 물리 파라미터 — RootComponent의 값만 백엔드에 적용 (compound shape 정책).
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Mass (kg)")
	float Mass = 1.0f;                          // kg
	UPROPERTY(Edit, Save, Category="Physics", DisplayName="Center Of Mass Offset")
	FVector CenterOfMassOffset = { 0, 0, 0 };   // RootComponent local 좌표계 offset
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Collision Enabled", Enum=ECollisionEnabled)
	ECollisionEnabled CollisionEnabled = ECollisionEnabled::NoCollision;
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Object Type", Enum=ECollisionChannel)
	ECollisionChannel ObjectType = ECollisionChannel::WorldStatic;
	UPROPERTY(Edit, Save, Category="Collision", DisplayName="Collision Responses", Type=Struct)
	FCollisionResponseContainer ResponseContainer; // 기본: 전 채널 Block
	FPrimitiveSceneProxy* SceneProxy = nullptr;

	FBodyInstance BodyInstance;

	FOctree* OctreeNode = nullptr;
	bool bInOctreeOverflow = false;
};
