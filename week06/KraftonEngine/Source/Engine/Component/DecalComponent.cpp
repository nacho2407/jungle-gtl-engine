#include "DecalComponent.h"
#include "Component/DecalComponent.h"
#include "Core/PropertyTypes.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/DecalSceneProxy.h"
#include "Resource/ResourceManager.h"
#include "Serialization/Archive.h"

#include <cstring>

IMPLEMENT_CLASS(UDecalComponent, UPrimitiveComponent)

void UDecalComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << TextureName;
	Ar << DecalColor;
	Ar << DecalSize;
	Ar << FadeInTime;
	Ar << FadeOutTime;
	Ar << TotalLifetime;
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
	OutProps.push_back({ "Fade In Time", EPropertyType::Float, &FadeInTime, 0.0f, 10.0f, 0.1f });
	OutProps.push_back({ "Fade Out Time", EPropertyType::Float, &FadeOutTime, 0.0f, 10.0f, 0.1f });
	OutProps.push_back({ "Total Lifetime", EPropertyType::Float, &TotalLifetime, 0.0f, 100.0f, 1.0f });
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

	ElapsedTime += DeltaTime;

	float CurrentFadeAlpha = 1.0f;

	if (FadeInTime > 0.0f)
	{
		CurrentFadeAlpha = std::min(CurrentFadeAlpha, ElapsedTime / FadeInTime);
	}

	if (TotalLifetime > 0.0f)
	{
		const float RemainingTime = TotalLifetime - ElapsedTime;
		if(FadeOutTime > 0.0f && RemainingTime < FadeOutTime)
		{
			CurrentFadeAlpha = std::min(CurrentFadeAlpha, RemainingTime / FadeOutTime);
		}

		if(RemainingTime <= 0.0f)
		{
			ResetFade();
			CurrentFadeAlpha = 0.0f;
		}
	}

	FadeAlpha = Clamp(CurrentFadeAlpha, 0.0f, 1.0f);
}

void UDecalComponent::SetTexture(const FName& InTextureName)
{
	TextureName = InTextureName;
	CachedTexture = FResourceManager::Get().FindTexture(InTextureName);
	// 텍스처 유무가 batcher/Primitive 경로 분기를 좌우하므로 Mesh 단계까지 재갱신 필요.
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
	ElapsedTime = 0.0f;
	FadeAlpha = 1.0f;

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
