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
		DynamicMaterial = CachedMesh->GetOrCreateDynamicMaterial(0);
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
		DynamicMaterial = CachedMesh ? CachedMesh->GetOrCreateDynamicMaterial(0) : nullptr;
		if (!DynamicMaterial)
		{
			return;
		}
	}

	FVector4 Color = GenerateRandomColor();
	if (!DynamicMaterial->SetVectorParameter("BaseColor", Color))
	{
		DynamicMaterial->SetVectorParameter("ColorTint", Color);
	}
}
