#include "Game/Component/CarDirtComponent.h"

#include "Game/Component/DirtComponent.h"
#include "GameFramework/AActor.h"
#include "Materials/MaterialManager.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCarDirtComponent, USceneComponent)

namespace
{
	constexpr int32 RequiredDirtCount = 5;

	FVector GetDefaultDirtLocation(int32 Index)
	{
		static const FVector Locations[RequiredDirtCount] = {
			FVector(0.05f, -0.68f, 0.55f),
			FVector(0.55f, -0.70f, 0.20f),
			FVector(-0.55f, -0.70f, 0.18f),
			FVector(0.08f, 0.72f, 0.45f),
			FVector(-0.78f, 0.05f, 0.35f)
		};
		return Locations[Index % RequiredDirtCount];
	}

	FRotator GetDefaultDirtRotation(int32 Index)
	{
		static const FRotator Rotations[RequiredDirtCount] = {
			FRotator(0.0f, 0.0f, 90.0f),
			FRotator(0.0f, 0.0f, 90.0f),
			FRotator(0.0f, 0.0f, 90.0f),
			FRotator(0.0f, 0.0f, -90.0f),
			FRotator(0.0f, 0.0f, 0.0f)
		};
		return Rotations[Index % RequiredDirtCount];
	}

	FVector GetDefaultDirtScale(int32 Index)
	{
		static const FVector Scales[RequiredDirtCount] = {
			FVector(0.95f, 0.55f, 0.55f),
			FVector(0.70f, 0.42f, 0.42f),
			FVector(0.62f, 0.38f, 0.38f),
			FVector(0.80f, 0.45f, 0.45f),
			FVector(0.52f, 0.36f, 0.36f)
		};
		return Scales[Index % RequiredDirtCount];
	}
}

void UCarDirtComponent::BeginPlay()
{
	USceneComponent::BeginPlay();
	EnsureDirtComponents();
}

void UCarDirtComponent::EnsureDirtComponents()
{
	while (CountDirtChildren() < RequiredDirtCount)
	{
		CreateDirtChild(CountDirtChildren());
	}
}

int32 UCarDirtComponent::CountDirtChildren() const
{
	int32 Count = 0;
	for (USceneComponent* Child : GetChildren())
	{
		if (Cast<UDirtComponent>(Child))
		{
			++Count;
		}
	}
	return Count;
}

UDirtComponent* UCarDirtComponent::CreateDirtChild(int32 Index)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	UDirtComponent* Dirt = OwnerActor->AddComponent<UDirtComponent>();
	if (!Dirt)
	{
		return nullptr;
	}

	Dirt->AttachToComponent(this);
	Dirt->SetRelativeLocation(GetDefaultDirtLocation(Index));
	Dirt->SetRelativeRotation(GetDefaultDirtRotation(Index));
	Dirt->SetRelativeScale(GetDefaultDirtScale(Index));
	Dirt->SetColor(FVector4(0.34f, 0.23f, 0.12f, 0.95f));
	Dirt->SetMaterial(FMaterialManager::Get().GetOrCreateMaterial("Asset/Materials/Auto/DirtDecal.mat"));
	return Dirt;
}
