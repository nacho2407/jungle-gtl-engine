#include "DirtComponent.h"

#include "Collision/RayUtils.h"
#include "Component/CompletionOutlineComponent.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"

#include <limits>

IMPLEMENT_CLASS(UDirtComponent, UDecalComponent)

UDirtComponent::UDirtComponent()
{
	SetColor(FVector4(DirtColor, CurrentAlpha));
	SetMaterial(FMaterialManager::Get().GetOrCreateMaterial("Asset/Materials/Auto/DirtDecal.mat"));
}

bool UDirtComponent::TryWashByRay(const FRay& Ray, float MaxDistance, float& OutDistance)
{
	if (bWashed || MaxDistance <= 0.0f || Ray.Direction.IsNearlyZero())
	{
		return false;
	}

	const FVector WorldRayEnd = Ray.Origin + Ray.Direction.Normalized() * MaxDistance;
	const FMatrix& WorldMatrix = GetWorldMatrix();
	const FMatrix& WorldInverse = GetWorldInverseMatrix();

	const FVector LocalOrigin = Ray.Origin * WorldInverse;
	const FVector LocalEnd = WorldRayEnd * WorldInverse;
	FVector LocalDirection = LocalEnd - LocalOrigin;
	if (LocalDirection.IsNearlyZero())
	{
		return false;
	}
	LocalDirection.Normalize();

	FRay LocalRay;
	LocalRay.Origin = LocalOrigin;
	LocalRay.Direction = LocalDirection;

	float LocalNear = 0.0f;
	float LocalFar = 0.0f;
	if (!FRayUtils::IntersectRayAABB(LocalRay, FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, 0.5f, 0.5f), LocalNear, LocalFar))
	{
		return false;
	}

	const float LocalHitDistance = LocalNear >= 0.0f ? LocalNear : LocalFar;
	if (LocalHitDistance < 0.0f)
	{
		return false;
	}

	const FVector LocalHit = LocalOrigin + LocalDirection * LocalHitDistance;
	const FVector WorldHit = LocalHit * WorldMatrix;
	OutDistance = FVector::Distance(Ray.Origin, WorldHit);
	return OutDistance <= MaxDistance;
}

void UDirtComponent::ApplyWashHit()
{
	if (bWashed)
	{
		return;
	}

	CurrentAlpha -= AlphaDecreasePerHit;
	if (CurrentAlpha <= 0.0f)
	{
		CurrentAlpha = 0.0f;
		SetColor(FVector4(DirtColor, CurrentAlpha));
		WashOff();
		return;
	}

	SetColor(FVector4(DirtColor, CurrentAlpha));
}

void UDirtComponent::WashOff()
{
	if (bWashed)
	{
		return;
	}

	bWashed = true;

	if (AActor* OwnerActor = GetOwner())
	{
		UCompletionOutlineComponent* Outline = OwnerActor->AddComponent<UCompletionOutlineComponent>();
		if (Outline)
		{
			if (USceneComponent* Parent = GetParent())
			{
				Outline->AttachToComponent(Parent);
			}

			Outline->SetRelativeLocation(GetRelativeLocation());
			Outline->SetRelativeRotation(GetRelativeRotation());
			Outline->SetRelativeScale(GetRelativeScale());
		}
	}

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->RemoveComponent(this);
	}
}

bool UDirtComponent::WashFirstHitDirt(UWorld* World, const FVector& Start, const FVector& Direction, float MaxDistance)
{
	if (!World || Direction.IsNearlyZero() || MaxDistance <= 0.0f)
	{
		return false;
	}

	FRay Ray;
	Ray.Origin = Start;
	Ray.Direction = Direction.Normalized();

	UDirtComponent* ClosestDirt = nullptr;
	float ClosestDistance = std::numeric_limits<float>::max();

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor)
		{
			continue;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			UDirtComponent* Dirt = Cast<UDirtComponent>(Component);
			if (!Dirt)
			{
				continue;
			}

			float HitDistance = 0.0f;
			if (Dirt->TryWashByRay(Ray, MaxDistance, HitDistance) && HitDistance < ClosestDistance)
			{
				ClosestDirt = Dirt;
				ClosestDistance = HitDistance;
			}
		}
	}

	if (!ClosestDirt)
	{
		return false;
	}

	ClosestDirt->ApplyWashHit();
	return true;
}

bool UDirtComponent::ShouldReceivePrimitive(UPrimitiveComponent* PrimitiveComp) const
{
	return PrimitiveComp && PrimitiveComp != this && Cast<UStaticMeshComponent>(PrimitiveComp) != nullptr;
}
