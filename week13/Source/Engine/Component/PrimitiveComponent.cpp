#include "PrimitiveComponent.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Core/Types/RayTypes.h"
#include "Collision/Ray/RayUtils.h"
#include "Collision/Octree/SpatialPartition.h"
#include "Physics/IPhysicsScene.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Core/Types/CollisionTypes.h"
#include "Render/Scene/FScene.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "GameFramework/World.h"
#include "Object/Reflection/ObjectFactory.h"

#include <cmath>
#include <cstring>

namespace
{
	bool HasSameTransformBasis(const FMatrix& A, const FMatrix& B)
	{
		for (int Row = 0; Row < 3; ++Row)
		{
			for (int Col = 0; Col < 3; ++Col)
			{
				if (A.M[Row][Col] != B.M[Row][Col])
				{
					return false;
				}
			}
		}

		return true;
	}

	bool IsNearlySameVector(const FVector& A, const FVector& B, float Tolerance = 1.0e-4f)
	{
		return std::abs(A.X - B.X) <= Tolerance
			&& std::abs(A.Y - B.Y) <= Tolerance
			&& std::abs(A.Z - B.Z) <= Tolerance;
	}
}

HIDE_FROM_COMPONENT_LIST(UPrimitiveComponent)

UPrimitiveComponent::UPrimitiveComponent()
{
	InitializeBodyInstance();
}

UPrimitiveComponent::~UPrimitiveComponent()
{
	if (UWorld* World = GetWorldEvenIfPendingKill())
	{
		if (IPhysicsScene* PS = World->GetPhysicsScene())
		{
			PS->UnregisterComponent(this);
		}
	}
	DestroyRenderState();
}

void UPrimitiveComponent::BeginPlay()
{
	USceneComponent::BeginPlay();

	InitializeBodyInstance();

	if (IsCollisionEnabled())
	{
		if (UWorld* World = GetWorld())
		{
			if (IPhysicsScene* PS = World->GetPhysicsScene())
			{
				PS->RegisterComponent(this);
			}
		}
	}

	CachedPhysicsWorldScale = GetWorldScale();
	bHasCachedPhysicsWorldScale = true;

	bComponentHasBegunPlay = true;
}

void UPrimitiveComponent::EndPlay()
{
	// World->DestroyActor → Actor::EndPlay → 컴포넌트 EndPlay 흐름. PhysX와 RenderState
	// (SceneProxy/Octree/PickingBVH)를 안전하게 정리하지 않으면 다음 frame에 stale 포인터를
	// 참조해 crash. dtor에도 같은 호출이 있지만 (raw 포인터라 OwnedComponents의 컴포넌트들이
	// 자동 delete되지 않아) dtor가 안 불릴 수 있어 EndPlay에서 명시적으로 보장한다.
	// 이중 호출은 mapping/proxy 부재로 noop.
	if (UWorld* World = GetWorldEvenIfPendingKill())
	{
		if (IPhysicsScene* PS = World->GetPhysicsScene())
		{
			PS->UnregisterComponent(this);
		}

		// SpatialPartition에서도 즉시 제거. World::DestroyActor가 Partition.RemoveActor를
		// 호출하지만, 그 시점에 OctreeNode 캐시가 이미 stale일 수 있는 경로(스폰 폭주 시
		// RebuildRootBounds 등)가 있어 EndPlay에서 한 번 더 보장한다. 중복 제거는 noop.
		World->GetPartition().RemoveSinglePrimitive(this);
	}
	// 캐시는 어떤 경로로도 다음 frame까지 살아있으면 안 된다 (FOctree node가 TryMerge로
	// 사라지면 dangling). RemoveSinglePrimitive 후에도 명시적으로 한 번 더 클리어.
	ClearOctreeLocation();

	DestroyRenderState();
	bComponentHasBegunPlay = false;
	bHasCachedPhysicsWorldScale = false;

	USceneComponent::EndPlay();
}

void UPrimitiveComponent::RouteComponentDestroyed()
{
    if (bComponentDestroyRouted)
    {
        return;
    }

    if (UWorld* World = GetWorldEvenIfPendingKill())
    {
        if (IPhysicsScene* PS = World->GetPhysicsScene())
        {
            PS->UnregisterComponent(this);
        }

        World->GetPartition().RemoveSinglePrimitive(this);
        World->MarkWorldPrimitivePickingBVHDirty();
    }

    ClearOctreeLocation();
    DestroyRenderState();

	bComponentHasBegunPlay = false;
	bHasCachedPhysicsWorldScale = false;

    USceneComponent::RouteComponentDestroyed();
}

void UPrimitiveComponent::BeginDestroy()
{
    if (HasAnyFlags(RF_BeginDestroy))
    {
        return;
    }

    RouteComponentDestroyed();
    USceneComponent::BeginDestroy();
}

void UPrimitiveComponent::NotifyPhysicsBodyDirty()
{
	if (!bComponentHasBegunPlay) return;

	UWorld* World = GetWorld();
	if (!World) return;

	IPhysicsScene* PS = World->GetPhysicsScene();
	if (!PS) return;

	if (IsCollisionEnabled()) PS->RebuildBody(this);
	else PS->UnregisterComponent(this);
}

void UPrimitiveComponent::SetSimulatePhysics(bool bInSimulate)
{
	if (bSimulatePhysics == bInSimulate) return;

	bSimulatePhysics = bInSimulate;
	BodyInstance.bSimulatePhysics = bInSimulate;

	NotifyPhysicsBodyDirty();
}

void UPrimitiveComponent::SetKinematicPhysics(bool bInKinematic)
{
	if (bKinematicPhysics == bInKinematic) return;

	bKinematicPhysics = bInKinematic;
	BodyInstance.bKinematic = bInKinematic;

	NotifyPhysicsBodyDirty();
}

void UPrimitiveComponent::SetEnableGravity(bool bInEnableGravity)
{
	if (bEnableGravity == bInEnableGravity && BodyInstance.bEnableGravity == bInEnableGravity) return;

	bEnableGravity = bInEnableGravity;
	BodyInstance.SetGravityEnabled(bInEnableGravity);
}

void UPrimitiveComponent::MarkProxyDirty(EDirtyFlag Flag) const
{
	if (!SceneProxy) return;
	if (UWorld* World = GetWorld())
	{
		World->GetScene().MarkProxyDirty(SceneProxy, Flag);
	}
}

void UPrimitiveComponent::SetVisibility(bool bNewVisible)
{
	if (bIsVisible == bNewVisible) return;
	bIsVisible = bNewVisible;
	MarkRenderVisibilityDirty();
}

void UPrimitiveComponent::SetCastShadow(bool bNewCastShadow)
{
	if (bCastShadow == bNewCastShadow) return;
	bCastShadow = bNewCastShadow;
	MarkRenderVisibilityDirty();
}

// ============================================================
// MarkRenderTransformDirty / MarkRenderVisibilityDirty
//   프록시 dirty + Octree(액터 단위 dirty) + PickingBVH dirty
//   호출자가 외워야 했던 시퀀스를 단일 진입점으로 통합.
// ============================================================
void UPrimitiveComponent::MarkRenderTransformDirty()
{
	MarkProxyDirty(EDirtyFlag::Transform);

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)) return;
	UWorld* World = OwnerActor->GetWorld();
	if (!World) return;

	World->UpdateActorInOctree(OwnerActor);
	World->MarkWorldPrimitivePickingBVHDirty();
}

void UPrimitiveComponent::MarkRenderVisibilityDirty()
{
	MarkProxyDirty(EDirtyFlag::Visibility);

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)) return;
	UWorld* World = OwnerActor->GetWorld();
	if (!World) return;

	// 가시성 변화는 Octree 포함 여부도 좌우하므로 액터 dirty로 반영한다.
	World->UpdateActorInOctree(OwnerActor);
	World->MarkWorldPrimitivePickingBVHDirty();
}

void UPrimitiveComponent::PostEditProperty(const char* PropertyName)
{
	// 베이스 클래스의 transform 등 공통 프로퍼티 처리 보장
	USceneComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "bIsVisible") == 0 || strcmp(PropertyName, "Visible") == 0)
	{
		// Property Editor가 bIsVisible을 직접 수정한 경우 dirty 시퀀스만 전파한다.
		MarkRenderVisibilityDirty();
	}
	else if (strcmp(PropertyName, "bCastShadow") == 0 || strcmp(PropertyName, "Cast Shadow") == 0)
	{
		MarkRenderVisibilityDirty();
	}
	else if (strcmp(PropertyName, "bCastShadowAsTwoSided") == 0 || strcmp(PropertyName, "Two Sided Shadow") == 0)
	{
		MarkRenderVisibilityDirty();
	}
	else if (strcmp(PropertyName, "CollisionEnabled") == 0 || strcmp(PropertyName, "Collision Enabled") == 0)
	{
		BodyInstance.CollisionEnabled = CollisionEnabled;

		if (!bComponentHasBegunPlay) return;

		if (UWorld* World = GetWorld())
		{
			if (IPhysicsScene* PS = World->GetPhysicsScene())
			{
				if (IsCollisionEnabled())
				{
					PS->RebuildBody(this);
				}
				else
				{
					PS->UnregisterComponent(this);
				}
			}
		}
	}
	else if (strcmp(PropertyName, "bSimulatePhysics") == 0 || strcmp(PropertyName, "Simulate Physics") == 0)
	{
		BodyInstance.bSimulatePhysics = bSimulatePhysics;
		NotifyPhysicsBodyDirty();
	}
	else if (strcmp(PropertyName, "bKinematicPhysics") == 0 || strcmp(PropertyName, "Kinematic Physics") == 0)
	{
		BodyInstance.bKinematic = bKinematicPhysics;
		NotifyPhysicsBodyDirty();
	}
	else if (strcmp(PropertyName, "bEnableGravity") == 0 || strcmp(PropertyName, "Enable Gravity") == 0)
	{
		SetEnableGravity(bEnableGravity);
	}
	else if (strcmp(PropertyName, "ObjectType") == 0 || strcmp(PropertyName, "Object Type") == 0)
	{
		BodyInstance.ObjectType = ObjectType;
		NotifyPhysicsBodyDirty();
	}
	else if (strcmp(PropertyName, "ResponseContainer") == 0 || strcmp(PropertyName, "Collision Responses") == 0)
	{
		BodyInstance.ResponseContainer = ResponseContainer;
		NotifyPhysicsBodyDirty();
	}
	else if (strcmp(PropertyName, "bGenerateOverlapEvents") == 0 || strcmp(PropertyName, "Generate Overlap Events") == 0)
	{
		// overlap notify 여부만 바뀌는 값이라 body rebuild는 필요 없음.
	}
	else if (strcmp(PropertyName, "Mass") == 0 || strcmp(PropertyName, "Mass (kg)") == 0)
	{
		// 에디터 슬라이더로 값을 바꾼 경우 백엔드에 즉시 반영.
		SetMass(Mass);
	}
	else if (strcmp(PropertyName, "CenterOfMassOffset") == 0 || strcmp(PropertyName, "Center Of Mass Offset") == 0)
	{
		SetCenterOfMass(CenterOfMassOffset);
	}
}

FBoundingBox UPrimitiveComponent::GetWorldBoundingBox() const
{
	EnsureWorldAABBUpdated();
	return FBoundingBox(WorldAABBMinLocation, WorldAABBMaxLocation);
}

void UPrimitiveComponent::MarkWorldBoundsDirty()
{
	// Local bounds(shape) 자체가 바뀐 경우용 진입점.
	// fast-path(이전 AABB를 translation만으로 재사용)는 shape가 동일하다는 가정에 의존하므로
	// 여기서는 반드시 무력화해야 한다. 안 그러면 mesh 교체 후에도 stale AABB가 캐시된다.
	bWorldAABBDirty = true;
	bHasValidWorldAABB = false;
	MarkRenderTransformDirty();
}

void UPrimitiveComponent::UpdateWorldAABB() const
{
	FVector LExt = LocalExtents;

	FMatrix worldMatrix = GetWorldMatrix();

	float NewEx = std::abs(worldMatrix.M[0][0]) * LExt.X + std::abs(worldMatrix.M[1][0]) * LExt.Y + std::abs(worldMatrix.M[2][0]) * LExt.Z;
	float NewEy = std::abs(worldMatrix.M[0][1]) * LExt.X + std::abs(worldMatrix.M[1][1]) * LExt.Y + std::abs(worldMatrix.M[2][1]) * LExt.Z;
	float NewEz = std::abs(worldMatrix.M[0][2]) * LExt.X + std::abs(worldMatrix.M[1][2]) * LExt.Y + std::abs(worldMatrix.M[2][2]) * LExt.Z;

	FVector WorldCenter = GetWorldLocation();
	WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
	WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

/* 현재 쓰이지 않는 코드입니다*/
// -> 쓰이고 있음
bool UPrimitiveComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	FMeshDataView View = GetMeshDataView();
	if (!View.IsValid()) return false;

	bool bHit = FRayUtils::RaycastTriangles(
		Ray, GetWorldMatrix(),
		GetWorldInverseMatrix(),
		View.VertexData,
		View.Stride,
		View.IndexData,
		View.IndexCount,
		OutHitResult);

	if (bHit)
	{
		OutHitResult.HitComponent = this;
	}
	return bHit;
}

void UPrimitiveComponent::UpdateWorldMatrix() const
{
	const FMatrix PreviousWorldMatrix = CachedWorldMatrix;
	const FVector PreviousWorldAABBMin = WorldAABBMinLocation;
	const FVector PreviousWorldAABBMax = WorldAABBMaxLocation;
	const bool bHadValidWorldAABB = bHasValidWorldAABB;

	USceneComponent::UpdateWorldMatrix();

	if (bWorldAABBDirty)
	{
		if (bHadValidWorldAABB && HasSameTransformBasis(PreviousWorldMatrix, CachedWorldMatrix))
		{
			const FVector TranslationDelta = CachedWorldMatrix.GetLocation() - PreviousWorldMatrix.GetLocation();
			WorldAABBMinLocation = PreviousWorldAABBMin + TranslationDelta;
			WorldAABBMaxLocation = PreviousWorldAABBMax + TranslationDelta;
			bWorldAABBDirty = false;
			bHasValidWorldAABB = true;
		}
		else
		{
			UpdateWorldAABB();
		}
	}

	// 프록시가 등록된 경우 Transform dirty 전파 (FScene DirtySet에도 등록)
	MarkProxyDirty(EDirtyFlag::Transform);
}

// --- 프록시 팩토리 ---
FPrimitiveSceneProxy* UPrimitiveComponent::CreateSceneProxy()
{
	// 기본 PrimitiveComponent용 프록시
	return new FPrimitiveSceneProxy(this);
}

// --- 렌더 상태 관리 (UE RegisterComponent 대응) ---
void UPrimitiveComponent::CreateRenderState()
{
	if (SceneProxy) return; // 이미 등록됨

	// Owner → World → FScene 경로로 접근
	UWorld* World = GetWorld();
	if (!World) return;

	// EditorOnly 컴포넌트는 에디터 월드에서만 프록시 생성
	if (IsEditorOnly() && World->GetWorldType() != EWorldType::Editor)
		return;

	FScene& Scene = World->GetScene();
	SceneProxy = Scene.AddPrimitive(this);
}

void UPrimitiveComponent::DestroyRenderState()
{
	// SceneProxy가 없더라도 Octree에는 등록돼 있을 수 있으므로 partition 정리는 항상 시도한다.
	if (UWorld* World = GetWorldEvenIfPendingKill())
		{
			World->GetPartition().RemoveSinglePrimitive(this);
			World->MarkWorldPrimitivePickingBVHDirty();

			if (SceneProxy)
			{
				// Scene.RemovePrimitive 가 VisibleProxies 캐시도 일관되게 정리한다.
				World->GetScene().RemovePrimitive(SceneProxy);
			}
	}
	SceneProxy = nullptr;
}

void UPrimitiveComponent::MarkRenderStateDirty()
{
	// 프록시 파괴 후 재생성 — 메시 교체 등 큰 변경 시 사용
	DestroyRenderState();
	CreateRenderState();
}


void UPrimitiveComponent::OnTransformDirty()
{
	// transform 변경 — local bounds(shape)는 그대로이므로 AABB fast-path는 살린다.
	// 단 PhysX shape geometry는 world scale 변경을 자동 반영하지 않으므로
	// scale이 바뀐 경우에만 body를 재생성한다. 이동/회전만으로는 rebuild하지 않는다.
	const bool bShouldCheckPhysicsScale = bComponentHasBegunPlay && IsCollisionEnabled();
	FVector NewWorldScale = FVector::OneVector;

	if (bShouldCheckPhysicsScale)
	{
		NewWorldScale = GetWorldScale();
		if (!bHasCachedPhysicsWorldScale)
		{
			CachedPhysicsWorldScale = NewWorldScale;
			bHasCachedPhysicsWorldScale = true;
		}
		else if (!IsNearlySameVector(CachedPhysicsWorldScale, NewWorldScale))
		{
			CachedPhysicsWorldScale = NewWorldScale;
			NotifyPhysicsBodyDirty();
		}
	}

	bWorldAABBDirty = true;
	MarkRenderTransformDirty();
}

void UPrimitiveComponent::EnsureWorldAABBUpdated() const
{
	GetWorldMatrix();
	if (bWorldAABBDirty)
	{
		UpdateWorldAABB();
	}
}

void UPrimitiveComponent::InitializeBodyInstance()
{
	if (BodyInstance.IsValidBodyInstance()) return;

	BodyInstance.OwnerComponent = this;
	BodyInstance.OwnerSkeletalComponent = nullptr;

	// 일반 PrimitiveComponent용
	BodyInstance.BoneName = FName::None;
	BodyInstance.BoneIndex = -1;

	BodyInstance.ClearPhysicsPointers();

	// 현재 컴포넌트 설정값을 기본 BodyInstance에도 맞춰둔다.
	BodyInstance.bSimulatePhysics = bSimulatePhysics;
	BodyInstance.bKinematic = bKinematicPhysics;
	BodyInstance.CollisionEnabled = CollisionEnabled;
	BodyInstance.ObjectType = ObjectType;
	BodyInstance.ResponseContainer = ResponseContainer;
	BodyInstance.Mass = Mass;
	BodyInstance.CenterOfMassOffset = CenterOfMassOffset;
	BodyInstance.bEnableGravity = bEnableGravity;
}

// --- Collision Channel / Response ---

void UPrimitiveComponent::SetCollisionEnabled(ECollisionEnabled InEnabled)
{
	if (CollisionEnabled == InEnabled) return;

	const bool bWasEnabled = IsCollisionEnabled();

	CollisionEnabled = InEnabled;
	BodyInstance.CollisionEnabled = InEnabled;

	const bool bIsEnabled = IsCollisionEnabled();

	if (!bComponentHasBegunPlay) return;

	UWorld* World = GetWorld();
	if (!World || !World->GetPhysicsScene()) return;

	if (!bWasEnabled && bIsEnabled)
	{
		World->GetPhysicsScene()->RegisterComponent(this);
	}
	else if (bWasEnabled && !bIsEnabled)
	{
		World->GetPhysicsScene()->UnregisterComponent(this);
	}
	else if (bWasEnabled && bIsEnabled)
	{
		// QueryOnly / PhysicsOnly / QueryAndPhysics 사이 전환은
		// PhysX shape flag가 달라지므로 재생성.
		NotifyPhysicsBodyDirty();
	}
}

bool UPrimitiveComponent::IsQueryCollisionEnabled() const
{
	return CollisionEnabled == ECollisionEnabled::QueryOnly
		|| CollisionEnabled == ECollisionEnabled::QueryAndPhysics;
}

bool UPrimitiveComponent::IsPhysicsCollisionEnabled() const
{
	return CollisionEnabled == ECollisionEnabled::PhysicsOnly
		|| CollisionEnabled == ECollisionEnabled::QueryAndPhysics;
}

void UPrimitiveComponent::SetCollisionObjectType(ECollisionChannel InChannel)
{
	if (ObjectType == InChannel) return;
	ObjectType = InChannel;
	BodyInstance.ObjectType = InChannel;
	NotifyPhysicsBodyDirty();
}

void UPrimitiveComponent::SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse Response)
{
	ResponseContainer.SetResponse(Channel, Response);
	BodyInstance.ResponseContainer = ResponseContainer;
	NotifyPhysicsBodyDirty();
}

void UPrimitiveComponent::SetCollisionResponseToAllChannels(ECollisionResponse Response)
{
	ResponseContainer.SetAllChannels(Response);
	BodyInstance.ResponseContainer = ResponseContainer;
	NotifyPhysicsBodyDirty();
}

ECollisionResponse UPrimitiveComponent::GetCollisionResponseToChannel(ECollisionChannel Channel) const
{
	return ResponseContainer.GetResponse(Channel);
}

ECollisionResponse UPrimitiveComponent::GetMinResponse(const UPrimitiveComponent* A, const UPrimitiveComponent* B)
{
	// 양쪽의 응답 중 더 제한적인(= 숫자가 작은) 쪽을 채택
	ECollisionResponse RespAtoB = A->GetCollisionResponseToChannel(B->GetCollisionObjectType());
	ECollisionResponse RespBtoA = B->GetCollisionResponseToChannel(A->GetCollisionObjectType());
	return (RespAtoB < RespBtoA) ? RespAtoB : RespBtoA;
}

// --- Overlap / Hit ---

// --- Physics Force/Velocity API ---

void UPrimitiveComponent::AddForce(const FVector& Force)
{
	if (UWorld* W = GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->AddForce(this, Force);
}

void UPrimitiveComponent::AddImpulse(const FVector& Impulse)
{
	if (UWorld* W = GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->AddImpulse(this, Impulse);
}

void UPrimitiveComponent::AddForceAtLocation(const FVector& Force, const FVector& Location)
{
	if (UWorld* W = GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->AddForceAtLocation(this, Force, Location);
}

void UPrimitiveComponent::AddTorque(const FVector& Torque)
{
	if (UWorld* W = GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->AddTorque(this, Torque);
}

FVector UPrimitiveComponent::GetLinearVelocity() const
{
	if (UWorld* W = GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				return PS->GetLinearVelocity(const_cast<UPrimitiveComponent*>(this));
	return { 0, 0, 0 };
}

void UPrimitiveComponent::SetLinearVelocity(const FVector& Vel)
{
	if (UWorld* W = GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->SetLinearVelocity(this, Vel);
}

FVector UPrimitiveComponent::GetAngularVelocity() const
{
	if (UWorld* W = GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				return PS->GetAngularVelocity(const_cast<UPrimitiveComponent*>(this));
	return { 0, 0, 0 };
}

void UPrimitiveComponent::SetAngularVelocity(const FVector& Vel)
{
	if (UWorld* W = GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->SetAngularVelocity(this, Vel);
}

void UPrimitiveComponent::SetMass(float NewMass)
{
	Mass = NewMass;
	BodyInstance.Mass = NewMass;
	if (UWorld* W = GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->SetMass(this, NewMass);
}

void UPrimitiveComponent::SetCenterOfMass(const FVector& LocalOffset)
{
	CenterOfMassOffset = LocalOffset;
	BodyInstance.CenterOfMassOffset = LocalOffset;
	if (UWorld* W = GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->SetCenterOfMass(this, LocalOffset);
}

FVector UPrimitiveComponent::GetCenterOfMass() const
{
	// 멤버 직접 반환 — 백엔드의 BodyState/Px와 SetCenterOfMass에서 동기화된다.
	// (백엔드 query를 거치면 RegisterComponent 내부에서 사용 시 fallback 루프 위험)
	return CenterOfMassOffset;
}

float UPrimitiveComponent::GetMass() const
{
	// 멤버 직접 반환 (위 GetCenterOfMass와 동일 이유).
	return Mass;
}

void UPrimitiveComponent::SetGenerateOverlapEvents(bool bInGenerateOverlapEvents)
{
	bGenerateOverlapEvents = bInGenerateOverlapEvents;
}

void UPrimitiveComponent::NotifyComponentBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	OnComponentBeginOverlap.Broadcast(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

void UPrimitiveComponent::NotifyComponentEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	OnComponentEndOverlap.Broadcast(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex);
}

void UPrimitiveComponent::NotifyComponentHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& HitResult)
{
	OnComponentHit.Broadcast(HitComponent, OtherActor, OtherComp, NormalImpulse, HitResult);
}

void UPrimitiveComponent::NotifyComponentEndHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp)
{
	OnComponentEndHit.Broadcast(HitComponent, OtherActor, OtherComp);
}
