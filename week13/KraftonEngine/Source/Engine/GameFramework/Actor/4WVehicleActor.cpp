#include "GameFramework/Actor/4WVehicleActor.h"

#include "Component/Movement/VehicleMovementComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "Component/SceneComponent.h"
#include "Component/Shape/BoxComponent.h"

namespace
{
	const FVector WheelOffsets[UVehicleMovementComponent::WheelCount] = {
		FVector(1.45f, 0.9f, -0.45f),
		FVector(1.45f, -0.9f, -0.45f),
		FVector(-1.45f, 0.9f, -0.45f),
		FVector(-1.45f, -0.9f, -0.45f),
	};

	const char* WheelPivotNames[UVehicleMovementComponent::WheelCount] = {
		"WheelPivot_FL",
		"WheelPivot_FR",
		"WheelPivot_RL",
		"WheelPivot_RR",
	};

	const char* WheelMeshNames[UVehicleMovementComponent::WheelCount] = {
		"Wheel_FL",
		"Wheel_FR",
		"Wheel_RL",
		"Wheel_RR",
	};
}

void A4WVehicleActor::BeginPlay()
{
	if (!GetRootComponent())
	{
		InitDefaultComponents();
	}
	else
	{
		RebindComponentReferences();
	}

	Super::BeginPlay();
}

void A4WVehicleActor::PostDuplicate()
{
	Super::PostDuplicate();
	RebindComponentReferences();
}

void A4WVehicleActor::InitDefaultComponents()
{
	if (GetRootComponent())
	{
		RebindComponentReferences();
		return;
	}

	ChassisCollision = AddComponent<UBoxComponent>();
	SetRootComponent(ChassisCollision);
	ChassisCollision->SetFName(FName("ChassisCollision"));
	ChassisCollision->SetBoxExtent(FVector(1.65f, 0.8f, 0.45f));
	ChassisCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ChassisCollision->SetCollisionObjectType(ECollisionChannel::WorldDynamic);
	ChassisCollision->SetSimulatePhysics(true);
	ChassisCollision->SetMass(1200.0f);
	ChassisCollision->SetCenterOfMass(FVector(0.0f, 0.0f, -0.3f));

	ChassisMesh = AddComponent<UStaticMeshComponent>();
	ChassisMesh->SetFName(FName("ChassisMesh"));
	ChassisMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChassisMesh->AttachToComponent(ChassisCollision);

	TArray<USceneComponent*> WheelSceneComponents;
	WheelSceneComponents.reserve(UVehicleMovementComponent::WheelCount);

	for (int32 WheelIndex = 0; WheelIndex < UVehicleMovementComponent::WheelCount; ++WheelIndex)
	{
		USceneComponent* WheelPivot = AddComponent<USceneComponent>();
		WheelPivot->SetFName(FName(WheelPivotNames[WheelIndex]));
		WheelPivot->AttachToComponent(ChassisCollision);
		WheelPivot->SetRelativeLocation(WheelOffsets[WheelIndex]);

		UStaticMeshComponent* WheelMesh = AddComponent<UStaticMeshComponent>();
		WheelMesh->SetFName(FName(WheelMeshNames[WheelIndex]));
		WheelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WheelMesh->AttachToComponent(WheelPivot);

		WheelPivots[WheelIndex] = WheelPivot;
		WheelMeshes[WheelIndex] = WheelMesh;
		WheelSceneComponents.push_back(WheelPivot);
	}

	VehicleMovementComponent = AddComponent<UVehicleMovementComponent>();
	VehicleMovementComponent->SetWheelSceneComponents(WheelSceneComponents);
}

void A4WVehicleActor::OnOwnedComponentRemoved(UActorComponent* Component)
{
	Super::OnOwnedComponentRemoved(Component);

	if (Component == ChassisCollision)
	{
		ChassisCollision = nullptr;
	}
	if (Component == ChassisMesh)
	{
		ChassisMesh = nullptr;
	}
	if (Component == VehicleMovementComponent)
	{
		VehicleMovementComponent = nullptr;
	}

	for (UStaticMeshComponent*& WheelMesh : WheelMeshes)
	{
		if (Component == WheelMesh)
		{
			WheelMesh = nullptr;
		}
	}

	for (USceneComponent*& WheelPivot : WheelPivots)
	{
		if (Component == WheelPivot)
		{
			WheelPivot = nullptr;
		}
	}
}

void A4WVehicleActor::RebindComponentReferences()
{
	ChassisCollision = Cast<UBoxComponent>(GetRootComponent());
	ChassisMesh = nullptr;
	for (USceneComponent*& WheelPivot : WheelPivots)
	{
		WheelPivot = nullptr;
	}
	for (UStaticMeshComponent*& WheelMesh : WheelMeshes)
	{
		WheelMesh = nullptr;
	}

	TArray<USceneComponent*> WheelSceneComponents;
	TArray<UStaticMeshComponent*> LegacyWheelMeshes;
	if (ChassisCollision)
	{
		for (USceneComponent* Child : ChassisCollision->GetChildren())
		{
			if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Child))
			{
				if (!ChassisMesh)
				{
					ChassisMesh = StaticMesh;
				}
				else
				{
					LegacyWheelMeshes.push_back(StaticMesh);
				}
				continue;
			}

			const size_t WheelIndex = WheelSceneComponents.size();
			if (WheelIndex >= UVehicleMovementComponent::WheelCount)
			{
				continue;
			}

			WheelPivots[WheelIndex] = Child;
			WheelSceneComponents.push_back(Child);

			for (USceneComponent* GrandChild : Child->GetChildren())
			{
				if (UStaticMeshComponent* WheelMesh = Cast<UStaticMeshComponent>(GrandChild))
				{
					WheelMeshes[WheelIndex] = WheelMesh;
					break;
				}
			}
		}

		for (UStaticMeshComponent* LegacyWheelMesh : LegacyWheelMeshes)
		{
			const size_t WheelIndex = WheelSceneComponents.size();
			if (WheelIndex >= UVehicleMovementComponent::WheelCount)
			{
				break;
			}

			USceneComponent* WheelPivot = AddComponent<USceneComponent>();
			WheelPivot->SetFName(FName(WheelPivotNames[WheelIndex]));
			WheelPivot->AttachToComponent(ChassisCollision);
			WheelPivot->SetRelativeLocation(LegacyWheelMesh->GetRelativeLocation());

			LegacyWheelMesh->AttachToComponent(WheelPivot);
			LegacyWheelMesh->SetRelativeLocation(FVector::ZeroVector);

			WheelPivots[WheelIndex] = WheelPivot;
			WheelMeshes[WheelIndex] = LegacyWheelMesh;
			WheelSceneComponents.push_back(WheelPivot);
		}
	}

	VehicleMovementComponent = GetComponentByClass<UVehicleMovementComponent>();
	if (VehicleMovementComponent)
	{
		VehicleMovementComponent->SetWheelSceneComponents(WheelSceneComponents);
	}
}
