#include "Component/DecalComponent.h"

#include <algorithm>
#include <cstring>

#include "Collision/RayUtils.h"
#include "Core/PropertyTypes.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/DecalSceneProxy.h"
#include "Resource/ResourceManager.h"
#include "Serialization/Archive.h"

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

UDecalComponent::UDecalComponent()
{
	SetDecalSize(DecalSize);
}

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
	SetDecalSize(DecalSize);
	SetFadeAlpha(FadeSetting.FadeAlpha);
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Texture", EPropertyType::Name, &TextureName });
	OutProps.push_back({ "Color", EPropertyType::Vec4, &DecalColor });
	OutProps.push_back({ "DecalSize", EPropertyType::Vec3, &DecalSize, 0.0f, 0.0f, 0.1f });
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
	else if (strcmp(PropertyName, "DecalSize") == 0)
	{
		SetDecalSize(DecalSize);
	}
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

	SetFadeAlpha(CurrentFadeAlpha);
}

bool UDecalComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	if (!IsHitTestEnabled())
	{
		return false;
	}

	const FMatrix& WorldInverse = GetWorldInverseMatrix();

	FRay LocalRay;
	LocalRay.Origin = WorldInverse.TransformPositionWithW(Ray.Origin);
	LocalRay.Direction = WorldInverse.TransformVector(Ray.Direction).Normalized();

	const FVector HalfExtents = GetHalfExtents();
	const FVector LocalMin = HalfExtents * -1.0f;
	const FVector LocalMax = HalfExtents;

	float HitTMin = 0.0f;
	float HitTMax = 0.0f;
	if (!FRayUtils::IntersectRayAABB(LocalRay, LocalMin, LocalMax, HitTMin, HitTMax))
	{
		return false;
	}

	const float HitT = (HitTMin >= 0.0f) ? HitTMin : HitTMax;
	if (HitT < 0.0f)
	{
		return false;
	}

	const FVector LocalHitPoint = LocalRay.Origin + LocalRay.Direction * HitT;
	const FVector WorldHitPoint = GetWorldMatrix().TransformPositionWithW(LocalHitPoint);

	OutHitResult.Distance = (WorldHitPoint - Ray.Origin).Length();
	OutHitResult.HitComponent = this;
	return true;
}

void UDecalComponent::SetTexture(const FName& InTextureName)
{
	TextureName = InTextureName;
	CachedTexture = TextureName.IsValid()
		? FResourceManager::Get().FindTexture(TextureName)
		: nullptr;
	MarkProxyDirty(EDirtyFlag::Material);
}

void UDecalComponent::SetFadeAlpha(float InFadeAlpha)
{
	FadeSetting.FadeAlpha = std::clamp(InFadeAlpha, 0.0f, 1.0f);
}

void UDecalComponent::SetDecalSize(const FVector& InDecalSize)
{
	DecalSize.X = std::max(InDecalSize.X, 0.01f);
	DecalSize.Y = std::max(InDecalSize.Y, 0.01f);
	DecalSize.Z = std::max(InDecalSize.Z, 0.01f);

	LocalExtents = GetHalfExtents();
	MarkDecalDirty();
	MarkRenderTransformDirty();
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

void UDecalComponent::GetWorldCorners(FVector(&OutCorners)[8]) const
{
	const FVector HalfExtents = GetHalfExtents();
	const FVector LocalCorners[8] = {
		FVector(-HalfExtents.X, -HalfExtents.Y, -HalfExtents.Z),
		FVector( HalfExtents.X, -HalfExtents.Y, -HalfExtents.Z),
		FVector( HalfExtents.X,  HalfExtents.Y, -HalfExtents.Z),
		FVector(-HalfExtents.X,  HalfExtents.Y, -HalfExtents.Z),
		FVector(-HalfExtents.X, -HalfExtents.Y,  HalfExtents.Z),
		FVector( HalfExtents.X, -HalfExtents.Y,  HalfExtents.Z),
		FVector( HalfExtents.X,  HalfExtents.Y,  HalfExtents.Z),
		FVector(-HalfExtents.X,  HalfExtents.Y,  HalfExtents.Z),
	};

	const FMatrix& WorldMatrix = GetWorldMatrix();
	for (int32 Index = 0; Index < 8; ++Index)
	{
		OutCorners[Index] = WorldMatrix.TransformPositionWithW(LocalCorners[Index]);
	}
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
