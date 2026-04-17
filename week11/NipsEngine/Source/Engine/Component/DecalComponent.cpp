#include "DecalComponent.h"

#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Core/ResourceManager.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "Object/ObjectFactory.h"

DEFINE_CLASS(UDecalComponent, UPrimitiveComponent)
REGISTER_FACTORY(UDecalComponent)

// Decal Box가 화면 밖으로 나가도 컬링되지 않도록 합니다.
UDecalComponent::UDecalComponent()
{
	Materials.resize(1);

	UMaterial* Mat = FResourceManager::Get().GetMaterial("DecalMat");
	SetMaterial(Mat);

	Mat->DepthStencilType = EDepthStencilType::Default;
	Mat->BlendType = EBlendType::AlphaBlend;
	Mat->RasterizerType = ERasterizerType::SolidBackCull;
	Mat->SamplerType = ESamplerType::EST_Linear;

    bEnableCull = false;
}

// Material 포인터는 프로퍼티 시스템에 노출되지 않으므로 직접 복사합니다.
// LifeTime 은 런타임 상태이므로 복사하지 않습니다 (BeginPlay 에서 0 으로 초기화).
void UDecalComponent::PostDuplicate(UObject* Original)
{
    UPrimitiveComponent::PostDuplicate(Original);

    const UDecalComponent* Orig = Cast<UDecalComponent>(Original);
	SetMaterial(Orig->GetMaterial()); // 얕은 복사 — ResourceManager 가 소유
}

void UDecalComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << "Material" << Materials[0]->GetNameRef();
	Ar << "Size" << DecalSize;
	Ar << "Color" << DecalColor;
	Ar << "Fade Start Delay" << FadeStartDelay;
	Ar << "Fade Duration" << FadeDuration;
	Ar << "Fade In Start Delay" << FadeInStartDelay;
	Ar << "Fade In Duration" << FadeInDuration;
	Ar << "Destroy Owner After Fade" << bDestroyOwnerAfterFade;

	if (Ar.IsLoading())
	{
		if (!Materials[0]->GetName().empty())
		{
			SetMaterial(FResourceManager::Get().GetMaterialInterface(Materials[0]->GetName()));
		}
	}
}

void UDecalComponent::BeginPlay()
{
	UPrimitiveComponent::BeginPlay();

	LifeTime = 0.0f;
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Materials", EPropertyType::Material, &Materials });
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

	if (std::strcmp(PropertyName, "Materials") == 0)
	{
		for (int32 i = 0; i < static_cast<int32>(Materials.size()); ++i)
		{
			if (Materials[i] == nullptr)
			{
				SetMaterial(i, FResourceManager::Get().GetMaterialInterface("DecalMat"));
				continue;
			}
			SetMaterial(i, Materials[i]);
		}
	}
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

	if (FadeInStartDelay + FadeInDuration > 0 && LifeTime < FadeInStartDelay + FadeInDuration)
	{
		TickFadeIn();
	}
	else if (FadeStartDelay + FadeDuration > 0 && LifeTime >= FadeInStartDelay + FadeInDuration)
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
	
	if (FadeInDuration <= 0.0f)
	{
		DecalColor.A = 1.0f;
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
			GetOwner()->GetFocusedWorld()->DestroyActor(GetOwner());
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
