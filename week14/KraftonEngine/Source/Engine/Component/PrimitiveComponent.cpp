#include "PrimitiveComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"
#include "Core/RayTypes.h"
#include "Collision/RayUtils.h"
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
	// CollisionSystem에서 안전하게 해제 (dangling pointer 방지)
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

	// 직렬화나 생성자에서 CollisionEnabled가 이미 설정된 경우 등록
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
	Ar << bGenerateOverlapEvents;
	Ar << CollisionEnabled;
	Ar << ObjectType;
	Ar << ResponseContainer;
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
		// 에디터에서 직접 enum 값을 변경한 경우 — 이미 CollisionEnabled 필드가 갱신된 상태
		// Register/Unregister를 수동으로 처리
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

	if (bWasQuery == bIsQuery) return;

	if (!Owner) return;
	UWorld* World = Owner->GetWorld();
	if (!World) return;

	if (bIsQuery)
	{
		World->GetPhysicsScene()->RegisterComponent(this);
	}
	else
	{
		World->GetPhysicsScene()->UnregisterComponent(this);
	}
}

bool UPrimitiveComponent::IsQueryCollisionEnabled() const
{
	return CollisionEnabled == ECollisionEnabled::QueryOnly
		|| CollisionEnabled == ECollisionEnabled::QueryAndPhysics;
}

void UPrimitiveComponent::SetCollisionObjectType(ECollisionChannel InChannel)
{
	ObjectType = InChannel;
}

void UPrimitiveComponent::SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse Response)
{
	ResponseContainer.SetResponse(Channel, Response);
}

void UPrimitiveComponent::SetCollisionResponseToAllChannels(ECollisionResponse Response)
{
	ResponseContainer.SetAllChannels(Response);
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
