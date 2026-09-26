#include "RandomColorComponent.h"
#include "MeshComponent.h"
#include "Actor/Actor.h"
#include "Object/Class.h"
#include <random>

IMPLEMENT_RTTI(URandomColorComponent, UActorComponent)

void URandomColorComponent::PostConstruct()
{
	bCanEverTick = true;
}

void URandomColorComponent::DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const
{
	UActorComponent::DuplicateShallow(DuplicatedObject, Context);

	URandomColorComponent* DuplicatedRandomColorComponent = static_cast<URandomColorComponent*>(DuplicatedObject);
	DuplicatedRandomColorComponent->CachedMesh = nullptr;
	DuplicatedRandomColorComponent->DynamicMaterial.reset();
	DuplicatedRandomColorComponent->UpdateInterval = UpdateInterval;
	DuplicatedRandomColorComponent->ElapsedTime = ElapsedTime;
}

void URandomColorComponent::FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const
{
	UActorComponent::FixupDuplicatedReferences(DuplicatedObject, Context);

	URandomColorComponent* DuplicatedRandomColorComponent = static_cast<URandomColorComponent*>(DuplicatedObject);
	DuplicatedRandomColorComponent->CachedMesh = Context.FindDuplicate(CachedMesh.Get());
}

URandomColorComponent::~URandomColorComponent() = default;

void URandomColorComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	if (Owner)
	{
		CachedMesh = Owner->GetComponentByClass<UMeshComponent>();
	}

	// 공유 Material을 복제하여 독립적인 DynamicMaterial 생성
	if (CachedMesh)
	{
		std::shared_ptr<FMaterial> BaseMat = CachedMesh->GetMaterial(0);
		if (BaseMat)
		{
			DynamicMaterial = std::shared_ptr<FDynamicMaterial>(BaseMat->CreateDynamicMaterial().release());
			if (DynamicMaterial)
			{
				CachedMesh->SetMaterial(0, DynamicMaterial);
			}
		}
	}

	// 시작 시 즉시 한 번 적용
	ApplyRandomColor();
}

void URandomColorComponent::Tick(float DeltaTime)
{
	ElapsedTime += DeltaTime;
	if (ElapsedTime >= UpdateInterval)
	{
		ElapsedTime -= UpdateInterval;
		ApplyRandomColor();
	}
}

namespace {
	FVector4 GenerateRandomColor()
	{
		static std::mt19937 Rng(std::random_device{}());
		static std::uniform_real_distribution<float> Dist(0.0f, 1.0f);
		return { Dist(Rng), Dist(Rng), Dist(Rng), 1.0f };
	}
}

void URandomColorComponent::ApplyRandomColor()
{
	if (!DynamicMaterial)
	{
		return;
	}

	FVector4 Color = GenerateRandomColor();
	if (!DynamicMaterial->SetVectorParameter("BaseColor", Color))
	{
		DynamicMaterial->SetVectorParameter("ColorTint", Color);
	}
}
