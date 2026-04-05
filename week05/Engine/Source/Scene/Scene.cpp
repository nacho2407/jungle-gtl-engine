#include "Scene.h"

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

IMPLEMENT_RTTI(UScene, UObject)

UScene::~UScene()
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


FCamera* UScene::GetCamera() const
{
	UWorld* World = GetTypedOuter<UWorld>();
	return World ? World->GetCamera() : nullptr;
}

EWorldType UScene::GetWorldType() const
{
	UWorld* World = GetTypedOuter<UWorld>();
	return World ? World->GetWorldType() : EWorldType::Game;
}

bool UScene::IsEditorScene() const
{
	return GetWorldType() == EWorldType::Editor;
}

bool UScene::IsGameScene() const
{
	const EWorldType WorldType = GetWorldType();
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}



void UScene::ClearActors()
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

void UScene::RegisterActor(AActor* InActor)
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
	InActor->SetScene(this);
	MarkSpatialDirty();
}

void UScene::DestroyActor(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}
	InActor->Destroy();
	MarkSpatialDirty();
}

void UScene::CleanupDestroyedActors()
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

void UScene::BeginPlay()
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

void UScene::Tick(float DeltaTime)
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

void UScene::MarkSpatialDirty()
{
	bSpatialDirty = true;
}

void UScene::GatherPrimitiveComponents(TArray<UPrimitiveComponent*>& OutPrimitives) const
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

void UScene::RebuildSpatialIfNeeded() const
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

void UScene::QueryPrimitivesByFrustum(const FFrustum& Frustum, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	RebuildSpatialIfNeeded();
	SpatialBVH.QueryFrustum(Frustum, OutPrimitives);
}

void UScene::QueryPrimitivesByRay(const FVector& RayOrigin, const FVector& RayDirection, float MaxDistance, TArray<UPrimitiveComponent*>& OutPrimitives) const
{
	RebuildSpatialIfNeeded();

	if (RayDirection.IsZero())
	{
		return;
	}

	const Ray SceneRay(RayOrigin, RayDirection.GetSafeNormal());
	SpatialBVH.QueryRay(SceneRay, MaxDistance, OutPrimitives);
}

void UScene::VisitPrimitivesByRay(const FVector& RayOrigin, const FVector& RayDirection, float& InOutMaxDistance, const BVH::FRayHitVisitor& Visitor) const
{
	RebuildSpatialIfNeeded();

	if (RayDirection.IsZero())
	{
		return;
	}

	const Ray SceneRay(RayOrigin, RayDirection.GetSafeNormal());
	SpatialBVH.VisitRay(SceneRay, InOutMaxDistance, Visitor);
}
