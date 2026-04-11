#include "Component/DecalComponent.h"

#include <algorithm>
#include <cstring>

#include "Collision/RayUtils.h"
#include "Object/ObjectFactory.h"
#include "Resource/ResourceManager.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UDecalComponent, UPrimitiveComponent)

UDecalComponent::UDecalComponent()
{
	SetDecalSize(DecalSize);
}

void UDecalComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << TextureName;
	Ar << FadeAlpha;
	Ar << DecalSize;
}

void UDecalComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();
	SetTexture(TextureName);
	SetDecalSize(DecalSize);
	SetFadeAlpha(FadeAlpha);
}

void UDecalComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Texture", EPropertyType::Name, &TextureName });
	OutProps.push_back({ "FadeAlpha", EPropertyType::Float, &FadeAlpha, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "DecalSize", EPropertyType::Vec3, &DecalSize, 0.0f, 0.0f, 0.1f });
}

void UDecalComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Texture") == 0)
	{
		SetTexture(TextureName);
	}
	else if (strcmp(PropertyName, "FadeAlpha") == 0)
	{
		SetFadeAlpha(FadeAlpha);
	}
	else if (strcmp(PropertyName, "DecalSize") == 0)
	{
		SetDecalSize(DecalSize);
	}
}

bool UDecalComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
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
}

void UDecalComponent::SetFadeAlpha(float InFadeAlpha)
{
	FadeAlpha = std::clamp(InFadeAlpha, 0.0f, 1.0f);
}

void UDecalComponent::SetDecalSize(const FVector& InDecalSize)
{
	DecalSize.X = std::max(InDecalSize.X, 0.01f);
	DecalSize.Y = std::max(InDecalSize.Y, 0.01f);
	DecalSize.Z = std::max(InDecalSize.Z, 0.01f);

	LocalExtents = GetHalfExtents();
	MarkWorldBoundsDirty();
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
