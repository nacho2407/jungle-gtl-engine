#include "DecalComponent.h"

#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Core/ResourceManager.h"
#include "Editor/UI/EditorConsoleWidget.h"

DEFINE_CLASS(UDecalComponent, UPrimitiveComponent)

UDecalComponent::UDecalComponent()
{
	const TArray<FString> MatNames = FResourceManager::Get().GetMaterialNames();
	SetMaterial(FResourceManager::Get().FindMaterial(MatNames[0]));
}

UDecalComponent* UDecalComponent::Duplicate()
{
	UDecalComponent* NewComp = UObjectManager::Get().CreateObject<UDecalComponent>();
	NewComp->SetActive(this->IsActive());
	NewComp->SetOwner(nullptr);
	
	NewComp->SetRelativeLocation(this->GetRelativeLocation());
	NewComp->SetRelativeRotation(this->GetRelativeRotation());
	NewComp->SetRelativeScale(this->GetRelativeScale());
	
	NewComp->SetVisibility(this->IsVisible());

	NewComp->Material = this->Material;
	NewComp->DecalSize = this->DecalSize;
	NewComp->DecalColor = this->DecalColor;
	
	NewComp->FadeStartDelay = this->FadeStartDelay;
	NewComp->FadeDuration = this->FadeDuration;
	NewComp->FadeInStartDelay = this->FadeInStartDelay;
	NewComp->FadeInDuration = this->FadeInDuration;
	NewComp->bDestroyOwnerAfterFade = this->bDestroyOwnerAfterFade;

	NewComp->DuplicateSubObjects();

	return NewComp;
}

void UDecalComponent::BeginPlay()
{
	UPrimitiveComponent::BeginPlay();

	LifeTime = 0.0f;
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Size", EPropertyType::Vec3, &DecalSize });
	OutProps.push_back({ "Color", EPropertyType::Vec4, &DecalColor });
	OutProps.push_back({ "Fade Start Delay", EPropertyType::Float, &FadeStartDelay });
	OutProps.push_back({ "Fade Duration", EPropertyType::Float, &FadeDuration });
	OutProps.push_back({ "Fade In Start Delay", EPropertyType::Float, &FadeInStartDelay });
	OutProps.push_back({ "Fade In Duration", EPropertyType::Float, &FadeInDuration });
	OutProps.push_back({ "Destroy Owner After Fade", EPropertyType::Bool, &bDestroyOwnerAfterFade });
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);
}

void UDecalComponent::UpdateWorldAABB() const
{
	// 월드 공간에서의 AABB 계산
	FVector WorldLocation = GetWorldLocation();
	FVector HalfSize = DecalSize * 0.5f;
	WorldAABB.Min = WorldLocation - HalfSize;
	WorldAABB.Max = WorldLocation + HalfSize;
}

bool UDecalComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
	return false;
}

FMatrix UDecalComponent::GetDecalMatrix() const
{
	FMatrix WorldMatrix = FMatrix::MakeScaleMatrix(DecalSize) * GetWorldMatrix();
	return WorldMatrix;
}

void UDecalComponent::TickComponent(float DeltaTime)
{
	UPrimitiveComponent::TickComponent(DeltaTime);

	LifeTime += DeltaTime;

	if (LifeTime < FadeInStartDelay + FadeInDuration)
	{
		TickFadeIn();
	}
	else
	{
		TickFadeOut();
	}
}

void UDecalComponent::TickFadeIn()
{
	float FadeInTime = LifeTime - FadeInStartDelay;
	if (FadeInTime < 0.0f)
	{
		DecalColor.A = 0.0f;
		return;
	}

	float Alpha = FadeInTime / FadeInDuration;

	DecalColor.A = MathUtil::Clamp(Alpha, 0.0f, 1.0f);
}

void UDecalComponent::TickFadeOut()
{
	float FadeOutLifeTime = LifeTime - FadeInStartDelay - FadeInDuration;

	float FadeOutTime = FadeOutLifeTime - FadeStartDelay;
	if (FadeOutTime < 0.0f) return;

	float Alpha = 1.0f - (FadeOutTime / FadeDuration);
	DecalColor.A = MathUtil::Clamp(Alpha, 0.0f, 1.0f);

	if (FadeOutLifeTime >= FadeStartDelay + FadeDuration)
	{
		SetActive(false);
		if (bDestroyOwnerAfterFade && GetOwner())
		{
			GetOwner()->GetWorld()->DestroyActor(GetOwner());
		}
	}
}

void UDecalComponent::SetFadeIn(float InStartDelay, float InDuration)
{
	FadeInStartDelay = InStartDelay;
	FadeInDuration = InDuration;
}

void UDecalComponent::SetFadeOut(float InStartDelay, float InDuration, bool bInDestroyOwnerAfterFade)
{
	FadeStartDelay = InStartDelay;
	FadeDuration = InDuration;
	bDestroyOwnerAfterFade = bInDestroyOwnerAfterFade;
}
