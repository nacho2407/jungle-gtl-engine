#include "Game/Component/DirtComponent.h"

#include "Collision/RayUtils.h"
#include "Game/Component/CompletionOutlineComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Debug/DrawDebugHelpers.h"
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

UStaticMeshComponent* UDirtComponent::FindWaterGunComponent(AActor& Actor)
{
	UStaticMeshComponent* FirstStaticMesh = nullptr;
	USceneComponent* RootComponent = Actor.GetRootComponent();
	if (UStaticMeshComponent* RootStaticMesh = Cast<UStaticMeshComponent>(RootComponent))
	{
		return RootStaticMesh;
	}

	for (UActorComponent* Component : Actor.GetComponents())
	{
		UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Component);
		if (!StaticMesh)
		{
			continue;
		}

		if (!FirstStaticMesh)
		{
			FirstStaticMesh = StaticMesh;
		}
	}

	return FirstStaticMesh;
}

UStaticMeshComponent* UDirtComponent::FindWaterStreamComponent(AActor& Actor)
{
	UStaticMeshComponent* WaterGun = FindWaterGunComponent(Actor);
	if (!WaterGun)
	{
		return nullptr;
	}

	for (USceneComponent* Child : WaterGun->GetChildren())
	{
		if (UStaticMeshComponent* Stream = Cast<UStaticMeshComponent>(Child))
		{
			return Stream;
		}
	}

	return nullptr;
}

bool UDirtComponent::FireCarWashRay(AActor& Actor)
{
	UWorld* World = Actor.GetWorld();
	if (!World)
	{
		return false;
	}

	UStaticMeshComponent* WaterStream = FindWaterStreamComponent(Actor);
	if (!WaterStream || !WaterStream->IsVisible())
	{
		return false;
	}

	UStaticMeshComponent* WaterGun = FindWaterGunComponent(Actor);
	const FVector Start = WaterStream->GetWorldLocation();

	FVector Direction = WaterStream->GetForwardVector();
	if (Direction.IsNearlyZero() && WaterGun)
	{
		Direction = WaterGun->GetForwardVector();
	}

	if (Direction.IsNearlyZero())
	{
		return false;
	}

	Direction.Normalize();
	constexpr float RayLength = 8.0f;
	DrawDebugLine(World, Start, Start + Direction * RayLength, FColor::Red(), 0.0f);
	return WashFirstHitDirt(World, Start, Direction, RayLength);
}

void UDirtComponent::SetCarWashStreamVisible(AActor& Actor, bool bVisible)
{
	if (UStaticMeshComponent* WaterStream = FindWaterStreamComponent(Actor))
	{
		WaterStream->SetVisibility(bVisible);
	}
}

bool UDirtComponent::IsCarWashStreamVisible(AActor& Actor)
{
	UStaticMeshComponent* WaterStream = FindWaterStreamComponent(Actor);
	return WaterStream && WaterStream->IsVisible();
}

bool UDirtComponent::AreAllDirtComponentsWashed(AActor& Actor)
{
	for (UActorComponent* Component : Actor.GetComponents())
	{
		if (UDirtComponent* Dirt = Cast<UDirtComponent>(Component))
		{
			if (!Dirt->IsWashed())
			{
				return false;
			}
		}
	}

	return true;
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
