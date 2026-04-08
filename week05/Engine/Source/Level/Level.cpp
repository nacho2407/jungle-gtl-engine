#include "Level.h"

#include "Core/Paths.h"
#include "Actor/Actor.h"
#include "Camera/Camera.h"
#include "Component/CameraComponent.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Object/Class.h"

#include "Serializer/SceneSerializer.h"
#include "World/World.h"
#include <algorithm>



#include "Component/LineBatchComponent.h"

IMPLEMENT_RTTI(ULevel, UObject)

ULevel::~ULevel()
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	Actors.clear();
	SpatialBVH.Reset();
	bSpatialDirty = true;
}


FCamera* ULevel::GetCamera() const
{
	UWorld* World = GetTypedOuter<UWorld>();
	return World ? World->GetCamera() : nullptr;
}

EWorldType ULevel::GetWorldType() const
{
	UWorld* World = GetTypedOuter<UWorld>();
	return World ? World->GetWorldType() : EWorldType::Game;
}

bool ULevel::IsEditorScene() const
{
	return GetWorldType() == EWorldType::Editor;
}

bool ULevel::IsGameScene() const
{
	const EWorldType WorldType = GetWorldType();
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULevel::DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	ULevel* DuplicatedLevel = static_cast<ULevel*>(DuplicatedObject);
	DuplicatedLevel->Actors.clear();
	DuplicatedLevel->bBegunPlay = false;
	DuplicatedLevel->SpatialBVH.Reset();
	DuplicatedLevel->bSpatialDirty = true;
}

void ULevel::DuplicateSubObjects(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	ULevel* DuplicatedLevel = static_cast<ULevel*>(DuplicatedObject);

	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor->IsPendingKill() || Actor->IsPendingDestroy())
		{
			continue;
		}

		AActor* DuplicatedActor = static_cast<AActor*>(Actor->Duplicate(DuplicatedLevel, Actor->GetName(), Context));
		if (!DuplicatedActor)
		{
			continue;
		}

		DuplicatedLevel->RegisterActor(DuplicatedActor);
	}
}

void ULevel::FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor->IsPendingKill() || Actor->IsPendingDestroy())
		{
			continue;
		}

		if (AActor* DuplicatedActor = Context.FindDuplicate(Actor))
		{
			Actor->FixupDuplicatedReferences(DuplicatedActor, Context);
		}
	}
}

void ULevel::PostDuplicate(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	ULevel* DuplicatedLevel = static_cast<ULevel*>(DuplicatedObject);

	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor->IsPendingKill() || Actor->IsPendingDestroy())
		{
			continue;
		}

		if (AActor* DuplicatedActor = Context.FindDuplicate(Actor))
		{
			Actor->PostDuplicate(DuplicatedActor, Context);
		}
	}

	DuplicatedLevel->MarkSpatialDirty();
}



void ULevel::ClearActors()
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	Actors.clear();

	bBegunPlay = false;
	MarkSpatialDirty();
}

void ULevel::RegisterActor(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	const auto It = std::find(Actors.begin(), Actors.end(), InActor);
	if (It != Actors.end())
	{
		return;
	}

	Actors.push_back(InActor);
	InActor->SetLevel(this);
	MarkSpatialDirty();
}

void ULevel::DestroyActor(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}
	InActor->Destroy();
	MarkSpatialDirty();
}

void ULevel::CleanupDestroyedActors()
{
	const auto NewEnd = std::ranges::remove_if(Actors,
		[](const AActor* Actor)
		{
			return Actor == nullptr || Actor->IsPendingDestroy();
		}).begin();

	const bool bRemovedAny = (NewEnd != Actors.end());
	Actors.erase(NewEnd, Actors.end());
	if (bRemovedAny)
	{
		MarkSpatialDirty();
	}
}

void ULevel::BeginPlay()
{
	if (bBegunPlay)
	{
		return;
	}

	bBegunPlay = true;

	for (AActor* Actor : Actors)
	{
		if (Actor && !Actor->HasBegunPlay())
		{
			Actor->BeginPlay();
		}
	}
}

void ULevel::Tick(float DeltaTime)
{
	if (!bBegunPlay)
	{
		BeginPlay();
	}

	for (AActor* Actor : Actors)
	{
		if (Actor && !Actor->IsPendingDestroy())
		{
			Actor->Tick(DeltaTime);
		}
	}

	CleanupDestroyedActors();
}

void ULevel::MarkSpatialDirty()
{
	bSpatialDirty = true;
}

void ULevel::GatherPrimitiveComponents(TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor->IsPendingDestroy())
		{
			continue;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component || !Component->IsA(UPrimitiveComponent::StaticClass()))
			{
				continue;
			}

			UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);
			if (PrimitiveComponent->IsPendingKill())
			{
				continue;
			}

			OutPrimitives.push_back(PrimitiveComponent);
		}
	}
}

void ULevel::RebuildSpatialIfNeeded() const
{
	if (!bSpatialDirty)
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GatherPrimitiveComponents(PrimitiveComponents);
	SpatialBVH.Build(PrimitiveComponents);
	bSpatialDirty = false;
}

void ULevel::QueryPrimitivesByFrustum(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	RebuildSpatialIfNeeded();
	SpatialBVH.QueryFrustum(Frustum, OutPrimitives);
}

void ULevel::QueryPrimitivesByRay(const FVector& RayOrigin, const FVector& RayDirection, float MaxDistance, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	RebuildSpatialIfNeeded();

	if (RayDirection.IsZero())
	{
		return;
	}

	const Ray SceneRay(RayOrigin, RayDirection.GetSafeNormal());
	SpatialBVH.QueryRay(SceneRay, MaxDistance, OutPrimitives);
}

void ULevel::VisitPrimitivesByRay(const FVector& RayOrigin, const FVector& RayDirection, float& InOutMaxDistance, const BVH::FRayHitVisitor& Visitor) const
{
	RebuildSpatialIfNeeded();

	if (RayDirection.IsZero())
	{
		return;
	}

	const Ray SceneRay(RayOrigin, RayDirection.GetSafeNormal());
	SpatialBVH.VisitRay(SceneRay, InOutMaxDistance, Visitor);
}

void ULevel::VisitBVHNodes(const FBVHNodeVisitor& Visitor) const
{
	RebuildSpatialIfNeeded();
	SpatialBVH.VisitNodes(Visitor);
}

void ULevel::VisitBVHNodesForPrimitive(UPrimitiveComponent* Target, const FBVHNodeVisitor& Visitor) const
{
	RebuildSpatialIfNeeded();
	SpatialBVH.VisitNodesForPrimitive(Target, Visitor);
}
