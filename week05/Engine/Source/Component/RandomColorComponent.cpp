#include "RandomColorComponent.h"
#include "PrimitiveComponent.h"
#include "Actor/Actor.h"
#include "Object/Class.h"
#include <random>

IMPLEMENT_RTTI(URandomColorComponent, UActorComponent)

void URandomColorComponent::Initialize()
{
	bCanEverTick = true;
}

URandomColorComponent::~URandomColorComponent() = default;

void URandomColorComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	if (Owner)
	{
		CachedPrimitive = Owner->GetComponentByClass<UPrimitiveComponent>();
	}

	// 공유 Material을 복제하여 독립적인 DynamicMaterial 생성
	if (CachedPrimitive && CachedPrimitive->GetMaterial())
	{
		DynamicMaterial = CachedPrimitive->GetMaterial()->CreateDynamicMaterial();
		if (DynamicMaterial)
		{
			CachedPrimitive->SetMaterial(DynamicMaterial.get());
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
