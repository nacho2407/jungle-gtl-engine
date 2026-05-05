#include "PrimitiveComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Core/RayTypes.h"
#include "Collision/RayUtils.h"
#include "Collision/SpatialPartition.h"
#include "Physics/IPhysicsScene.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Core/CollisionTypes.h"
#include "Render/Scene/FScene.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"

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
}

IMPLEMENT_CLASS(UPrimitiveComponent, USceneComponent)
HIDE_FROM_COMPONENT_LIST(UPrimitiveComponent)

UPrimitiveComponent::~UPrimitiveComponent()
{
	if (Owner)
	{
		if (UWorld* World = Owner->GetWorld())
		{
			World->GetPhysicsScene()->UnregisterComponent(this);
		}
	}
	DestroyRenderState();
}

void UPrimitiveComponent::BeginPlay()
{
	USceneComponent::BeginPlay();

	// 직렬화나 InitDefaultComponents에서 CollisionEnabled가 이미 설정된 경우 등록.
	// 이 시점에 SimulatePhysics/ObjectType/Response/Mass/COM 등 모든 셋업이 끝나있어
	// PhysX/Native가 정확한 값으로 body를 생성한다.
	if (IsQueryCollisionEnabled())
	{
		if (Owner)
		{
			if (UWorld* World = Owner->GetWorld())
			{
				World->GetPhysicsScene()->RegisterComponent(this);
			}
		}
	}

	// flag는 등록 흐름이 끝난 직후에만 true. 이후 setter들이 PhysicsScene::RebuildBody 호출.
	bComponentHasBegunPlay = true;
}

void UPrimitiveComponent::EndPlay()
{
	// World->DestroyActor → Actor::EndPlay → 컴포넌트 EndPlay 흐름. PhysX와 RenderState
	// (SceneProxy/Octree/PickingBVH)를 안전하게 정리하지 않으면 다음 frame에 stale 포인터를
	// 참조해 crash. dtor에도 같은 호출이 있지만 (raw 포인터라 OwnedComponents의 컴포넌트들이
	// 자동 delete되지 않아) dtor가 안 불릴 수 있어 EndPlay에서 명시적으로 보장한다.
	// 이중 호출은 mapping/proxy 부재로 noop.
	if (Owner)
	{
		if (UWorld* World = Owner->GetWorld())
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
	}
	// 캐시는 어떤 경로로도 다음 frame까지 살아있으면 안 된다 (FOctree node가 TryMerge로
	// 사라지면 dangling). RemoveSinglePrimitive 후에도 명시적으로 한 번 더 클리어.
	ClearOctreeLocation();

	DestroyRenderState();
	bComponentHasBegunPlay = false;

	USceneComponent::EndPlay();
}

void UPrimitiveComponent::NotifyPhysicsBodyDirty()
{
	if (!bComponentHasBegunPlay) return;
	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;
	if (IPhysicsScene* PS = World->GetPhysicsScene())
	{
		PS->RebuildBody(this);
	}
}

void UPrimitiveComponent::SetSimulatePhysics(bool bInSimulate)
{
	if (bSimulatePhysics == bInSimulate) return;
	bSimulatePhysics = bInSimulate;
	NotifyPhysicsBodyDirty();
}

void UPrimitiveComponent::MarkProxyDirty(EDirtyFlag Flag) const
{
	if (!SceneProxy || !Owner || !Owner->GetWorld()) return;
	Owner->GetWorld()->GetScene().MarkProxyDirty(SceneProxy, Flag);
}

void UPrimitiveComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << bIsVisible;
	Ar << bCastShadow;
	Ar << bCastShadowAsTwoSided;
	Ar << bSimulatePhysics;
	Ar << bGenerateOverlapEvents;
	Ar << CollisionEnabled;
	Ar << ObjectType;
	Ar << ResponseContainer;
	Ar << Mass;
	Ar << CenterOfMassOffset;
	// LocalExtents는 메시 등에서 재계산되므로 직렬화 제외.
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
	if (!OwnerActor) return;
	UWorld* World = OwnerActor->GetWorld();
	if (!World) return;

	World->UpdateActorInOctree(OwnerActor);
	World->MarkWorldPrimitivePickingBVHDirty();
}

void UPrimitiveComponent::MarkRenderVisibilityDirty()
{
	MarkProxyDirty(EDirtyFlag::Visibility);

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;
	UWorld* World = OwnerActor->GetWorld();
	if (!World) return;

	// 가시성 변화는 Octree 포함 여부도 좌우하므로 액터 dirty로 반영한다.
	World->UpdateActorInOctree(OwnerActor);
	World->MarkWorldPrimitivePickingBVHDirty();
}

void UPrimitiveComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "Visible", EPropertyType::Bool, "Rendering", &bIsVisible });
	OutProps.push_back({ "Cast Shadow", EPropertyType::Bool, "Rendering", &bCastShadow });
	OutProps.push_back({ "Two Sided Shadow", EPropertyType::Bool, "Rendering", &bCastShadowAsTwoSided });

	OutProps.push_back({ "Simulate Physics", EPropertyType::Bool, "Collision", &bSimulatePhysics });
	OutProps.push_back({ "Generate Overlap Events", EPropertyType::Bool, "Collision", &bGenerateOverlapEvents });

	{
		FPropertyDescriptor Desc;
		Desc.Name = "Collision Enabled";
		Desc.Type = EPropertyType::Enum;
		Desc.Category = "Collision";
		Desc.ValuePtr = &CollisionEnabled;
		Desc.EnumNames = GCollisionEnabledNames;
		Desc.EnumCount = static_cast<uint32>(ECollisionEnabled::COUNT);
		Desc.EnumSize = sizeof(ECollisionEnabled);
		OutProps.push_back(Desc);
	}

	{
		FPropertyDescriptor Desc;
		Desc.Name = "Object Type";
		Desc.Type = EPropertyType::Enum;
		Desc.Category = "Collision";
		Desc.ValuePtr = &ObjectType;
		Desc.EnumNames = GCollisionChannelNames;
		Desc.EnumCount = static_cast<uint32>(ECollisionChannel::ActiveCount);
		Desc.EnumSize = sizeof(ECollisionChannel);
		OutProps.push_back(Desc);
	}

	{
		FPropertyDescriptor Desc;
		Desc.Name = "Collision Responses";
		Desc.Type = EPropertyType::Struct;
		Desc.Category = "Collision";
		Desc.ValuePtr = &ResponseContainer;
		Desc.StructFunc = &FCollisionResponseContainer::DescribeProperties;
		OutProps.push_back(Desc);
	}

	// 물리 파라미터 — RootComponent에 한해 백엔드에 적용된다 (compound shape).
	OutProps.push_back({ "Mass (kg)", EPropertyType::Float, "Physics", &Mass });
	OutProps.push_back({ "Center Of Mass Offset", EPropertyType::Vec3, "Physics", &CenterOfMassOffset });
}

void UPrimitiveComponent::PostEditProperty(const char* PropertyName)
{
	// 베이스 클래스의 transform 등 공통 프로퍼티 처리 보장
	USceneComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Visible") == 0)
	{
		// Property Editor가 bIsVisible을 직접 수정한 경우 dirty 시퀀스만 전파한다.
		MarkRenderVisibilityDirty();
	}
	else if (strcmp(PropertyName, "Cast Shadow") == 0)
	{
		MarkRenderVisibilityDirty();
	}
	else if (strcmp(PropertyName, "Two Sided Shadow") == 0)
	{
		MarkRenderVisibilityDirty();
	}
	else if (strcmp(PropertyName, "Collision Enabled") == 0)
	{
		// 에디터 property panel 이 enum 값을 직접 바꾼 경우 — 이미 CollisionEnabled 필드는
		// 갱신된 상태고 setter 를 안 거쳤으니 여기서 Register/Unregister 처리.
		//
		// 단 BeginPlay 이전에는 skip — DeserializeProperties 가 GetEditableProperties
		// 순서대로 프로퍼티를 set 하면서 매번 PostEditProperty 를 부르는데, "Collision
		// Enabled" 는 그 중에 비교적 앞쪽에 위치해서 ObjectType / Mass / COM / BoxExtent
		// 같은 다른 프로퍼티들이 아직 default 인 시점에 RegisterComponent 가 발화해버린다.
		// 결과: 단위 큐브 + mass=1 + WorldStatic 으로 PhysX 본체가 만들어지고, 이후 다른
		// 프로퍼티 setter 들의 NotifyPhysicsBodyDirty 가 같은 가드에 막혀 no-op 라 영영
		// 갱신 안 됨. BeginPlay 의 RegisterComponent 한 번에 위임하면 모든 프로퍼티가
		// 최종 상태인 채로 등록된다 (PIE Duplicate 경로와 동일한 타이밍).
		if (!bComponentHasBegunPlay) return;

		if (Owner)
		{
			if (UWorld* World = Owner->GetWorld())
			{
				if (IsQueryCollisionEnabled())
				{
					World->GetPhysicsScene()->RegisterComponent(this);
				}
				else
				{
					World->GetPhysicsScene()->UnregisterComponent(this);
				}
			}
		}
	}
	else if (strcmp(PropertyName, "Mass (kg)") == 0)
	{
		// 에디터 슬라이더로 값을 바꾼 경우 백엔드에 즉시 반영.
		SetMass(Mass);
	}
	else if (strcmp(PropertyName, "Center Of Mass Offset") == 0)
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
	if (!Owner || !Owner->GetWorld()) return;

	// EditorOnly 컴포넌트는 에디터 월드에서만 프록시 생성
	if (IsEditorOnly() && Owner->GetWorld()->GetWorldType() != EWorldType::Editor)
		return;

	FScene& Scene = Owner->GetWorld()->GetScene();
	SceneProxy = Scene.AddPrimitive(this);
}

void UPrimitiveComponent::DestroyRenderState()
{
	// SceneProxy가 없더라도 Octree에는 등록돼 있을 수 있으므로 partition 정리는 항상 시도한다.
	if (Owner)
	{
		if (UWorld* World = Owner->GetWorld())
		{
			World->GetPartition().RemoveSinglePrimitive(this);
			World->MarkWorldPrimitivePickingBVHDirty();

			if (SceneProxy)
			{
				// Scene.RemovePrimitive 가 VisibleProxies 캐시도 일관되게 정리한다.
				World->GetScene().RemovePrimitive(SceneProxy);
			}
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
	// 순수 transform 변경 — local bounds(shape)는 그대로이므로 fast-path를 살린다.
	// (basis 동일 + translation만 바뀐 경우 UpdateWorldMatrix가 이전 AABB를 평행이동만 적용)
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

// --- Collision Channel / Response ---

void UPrimitiveComponent::SetCollisionEnabled(ECollisionEnabled InEnabled)
{
	bool bWasQuery = IsQueryCollisionEnabled();
	CollisionEnabled = InEnabled;
	bool bIsQuery = IsQueryCollisionEnabled();

	// 컴포넌트 BeginPlay 전이면 멤버만 변경. BeginPlay에서 한 번 등록되며 그 시점엔
	// SimulatePhysics 등 다른 셋업이 모두 완료된 상태.
	if (!bComponentHasBegunPlay) return;

	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	if (bWasQuery != bIsQuery)
	{
		if (bIsQuery)
			World->GetPhysicsScene()->RegisterComponent(this);
		else
			World->GetPhysicsScene()->UnregisterComponent(this);
	}
	else if (bWasQuery && bIsQuery)
	{
		// 이미 등록된 상태에서 enabled 종류 변경 (예: QueryOnly ↔ QueryAndPhysics) — 재구성
		NotifyPhysicsBodyDirty();
	}
}

bool UPrimitiveComponent::IsQueryCollisionEnabled() const
{
	return CollisionEnabled == ECollisionEnabled::QueryOnly
		|| CollisionEnabled == ECollisionEnabled::QueryAndPhysics;
}

void UPrimitiveComponent::SetCollisionObjectType(ECollisionChannel InChannel)
{
	if (ObjectType == InChannel) return;
	ObjectType = InChannel;
	NotifyPhysicsBodyDirty();
}

void UPrimitiveComponent::SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse Response)
{
	ResponseContainer.SetResponse(Channel, Response);
	NotifyPhysicsBodyDirty();
}

void UPrimitiveComponent::SetCollisionResponseToAllChannels(ECollisionResponse Response)
{
	ResponseContainer.SetAllChannels(Response);
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
	if (Owner)
		if (UWorld* W = Owner->GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->AddForce(this, Force);
}

void UPrimitiveComponent::AddForceAtLocation(const FVector& Force, const FVector& Location)
{
	if (Owner)
		if (UWorld* W = Owner->GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->AddForceAtLocation(this, Force, Location);
}

void UPrimitiveComponent::AddTorque(const FVector& Torque)
{
	if (Owner)
		if (UWorld* W = Owner->GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->AddTorque(this, Torque);
}

FVector UPrimitiveComponent::GetLinearVelocity() const
{
	if (Owner)
		if (UWorld* W = Owner->GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				return PS->GetLinearVelocity(const_cast<UPrimitiveComponent*>(this));
	return { 0, 0, 0 };
}

void UPrimitiveComponent::SetLinearVelocity(const FVector& Vel)
{
	if (Owner)
		if (UWorld* W = Owner->GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->SetLinearVelocity(this, Vel);
}

FVector UPrimitiveComponent::GetAngularVelocity() const
{
	if (Owner)
		if (UWorld* W = Owner->GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				return PS->GetAngularVelocity(const_cast<UPrimitiveComponent*>(this));
	return { 0, 0, 0 };
}

void UPrimitiveComponent::SetAngularVelocity(const FVector& Vel)
{
	if (Owner)
		if (UWorld* W = Owner->GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->SetAngularVelocity(this, Vel);
}

void UPrimitiveComponent::SetMass(float NewMass)
{
	Mass = NewMass;
	if (Owner)
		if (UWorld* W = Owner->GetWorld())
			if (IPhysicsScene* PS = W->GetPhysicsScene())
				PS->SetMass(this, NewMass);
}

void UPrimitiveComponent::SetCenterOfMass(const FVector& LocalOffset)
{
	CenterOfMassOffset = LocalOffset;
	if (Owner)
		if (UWorld* W = Owner->GetWorld())
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
