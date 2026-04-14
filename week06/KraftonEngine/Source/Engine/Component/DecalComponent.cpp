#include "DecalComponent.h"

#include "Core/PropertyTypes.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/DecalSceneProxy.h"
#include "Resource/ResourceManager.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cstring>

namespace
{
	void DeselectOwnerIfSelected(AActor* OwnerActor)
	{
		if (!OwnerActor)
		{
			return;
		}

		UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
		if (!Editor)
		{
			return;
		}

		FSelectionManager& Selection = Editor->GetSelectionManager();
		if (Selection.IsSelected(OwnerActor))
		{
			Selection.Deselect(OwnerActor);
		}
	}
}

IMPLEMENT_CLASS(UDecalComponent, UPrimitiveComponent)

void UDecalComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << TextureName;
	Ar << DecalColor;
	Ar << DecalSize;
	Ar << FadeSetting.FadeInTime;
	Ar << FadeSetting.FadeOutTime;
	Ar << FadeSetting.TotalLifetime;
	Ar << FadeSetting.bFadeOnceAndDestroy;
}

void UDecalComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();
	SetTexture(TextureName);
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Texture", EPropertyType::Name, &TextureName });
	OutProps.push_back({ "Color", EPropertyType::Vec4, &DecalColor });
	OutProps.push_back({ "Decal Size", EPropertyType::Vec3, &DecalSize, 1.0f, 1000.0f, 1.0f });
	OutProps.push_back({ "Fade In Time", EPropertyType::Float, &FadeSetting.FadeInTime, 0.0f, 10.0f, 0.1f });
	OutProps.push_back({ "Fade Out Time", EPropertyType::Float, &FadeSetting.FadeOutTime, 0.0f, 10.0f, 0.1f });
	OutProps.push_back({ "Total Lifetime", EPropertyType::Float, &FadeSetting.TotalLifetime, 0.0f, 100.0f, 1.0f });
	OutProps.push_back({ "Fade Once And Destroy", EPropertyType::Bool, &FadeSetting.bFadeOnceAndDestroy });
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Texture") == 0)
	{
		SetTexture(TextureName);
	}
	else if (strcmp(PropertyName, "Decal Size") == 0)
	{
		UpdateLocalExtents();
		MarkDecalDirty();
		MarkWorldBoundsDirty();
	}
}

void UDecalComponent::UpdateLocalExtents()
{
	LocalExtents = DecalSize * 0.5f;
}

FPrimitiveSceneProxy* UDecalComponent::CreateSceneProxy()
{
	return new FDecalSceneProxy(this);
}

void UDecalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UPrimitiveComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (FadeSetting.TotalLifetime <= 0.0f)
	{
		return;
	}

	FadeSetting.ElapsedTime += DeltaTime;

	float CurrentFadeAlpha = 1.0f;
	if (FadeSetting.FadeInTime > 0.0f)
	{
		CurrentFadeAlpha = std::min(CurrentFadeAlpha, FadeSetting.ElapsedTime / FadeSetting.FadeInTime);
	}

	const float RemainingTime = FadeSetting.TotalLifetime - FadeSetting.ElapsedTime;
	if (FadeSetting.FadeOutTime > 0.0f && RemainingTime < FadeSetting.FadeOutTime)
	{
		CurrentFadeAlpha = std::min(CurrentFadeAlpha, RemainingTime / FadeSetting.FadeOutTime);
	}

	if (RemainingTime <= 0.0f)
	{
		if (FadeSetting.bFadeOnceAndDestroy)
		{
			DeselectOwnerIfSelected(Owner);
			SetActive(false);
			SetComponentTickEnabled(false);
			
			if (Owner && Owner->GetWorld())
			{
				Owner->GetWorld()->DestroyActor(Owner);
			}
			return;
		}

		ResetFade();
		CurrentFadeAlpha = 0.0f;
	}

	FadeSetting.FadeAlpha = Clamp(CurrentFadeAlpha, 0.0f, 1.0f);
}

void UDecalComponent::SetTexture(const FName& InTextureName)
{
	TextureName = InTextureName;
	CachedTexture = FResourceManager::Get().FindTexture(InTextureName);
	MarkProxyDirty(EDirtyFlag::Material);
}

void UDecalComponent::SetDecalSize(const FVector& InSize)
{
	DecalSize = InSize;
	MarkDecalDirty();
	MarkRenderTransformDirty();
	UpdateLocalExtents();
	MarkWorldBoundsDirty();
}

void UDecalComponent::ResetFade()
{
	FadeSetting.Reset();
	SetActive(true);
	SetComponentTickEnabled(true);
}

FMatrix& UDecalComponent::GetDecalToWorldMatrix() const
{
	if (bDecalMatrixDirty)
	{
		UpdateDecalMatrices();
	}
	return DecalToWorld;
}

FMatrix& UDecalComponent::GetWorldToDecalMatrix() const
{
	if (bDecalMatrixDirty)
	{
		UpdateDecalMatrices();
	}
	return WorldToDecal;
}

void UDecalComponent::OnTransformDirty()
{
	UPrimitiveComponent::OnTransformDirty();
	MarkDecalDirty();
}

void UDecalComponent::MarkDecalDirty()
{
	bDecalMatrixDirty = true;
}

void UDecalComponent::UpdateDecalMatrices() const
{
	DecalToWorld = GetWorldMatrix();
	const FMatrix ScaleMatrix = FMatrix::MakeScaleMatrix(DecalSize);
	DecalToWorld = ScaleMatrix * DecalToWorld;
	WorldToDecal = DecalToWorld.GetInverse();
	bDecalMatrixDirty = false;
}
