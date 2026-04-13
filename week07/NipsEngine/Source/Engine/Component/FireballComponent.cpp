#include "FireballComponent.h"
DEFINE_CLASS(UFireballComponent, UPrimitiveComponent)
REGISTER_FACTORY(UFireballComponent)

// 화면에서 컬링되지 않도록 수정한다.
UFireballComponent::UFireballComponent()
{
	bEnableCull = false;
}

void UFireballComponent::PostDuplicate(UObject* Original)
{
    UPrimitiveComponent::PostDuplicate(Original);

    const UPrimitiveComponent* Orig = Cast<UFireballComponent>(Original);
}

void UFireballComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);

    OutProps.push_back({ "Radius", EPropertyType::Float, &Radius });
    OutProps.push_back({ "Radius Falloff", EPropertyType::Float, &RadiusFallOff });
    OutProps.push_back({ "Intensity", EPropertyType::Float, &Intensity });
    OutProps.push_back({ "Red", EPropertyType::Float, &Color.R });
    OutProps.push_back({ "Green", EPropertyType::Float, &Color.G });
    OutProps.push_back({ "Blue", EPropertyType::Float, &Color.B });
}

void UFireballComponent::PostEditProperty(const char* PropertyName)
{
    UPrimitiveComponent::PostEditProperty(PropertyName);
}

void UFireballComponent::UpdateWorldAABB() const
{
    WorldAABB.Reset();
    float TotalWidth = 0.5f;
    float TotalHeight = 0.5f;

    FVector WorldScale = GetWorldScale();
    float ScaledWidth = TotalWidth * WorldScale.Y;
    float ScaledHeight = TotalHeight * WorldScale.Z;

    FVector WorldRight = FVector(CachedWorldMatrix.M[1][0], CachedWorldMatrix.M[1][1], CachedWorldMatrix.M[1][2]).Normalized();
    FVector WorldUp = FVector(CachedWorldMatrix.M[2][0], CachedWorldMatrix.M[2][1], CachedWorldMatrix.M[2][2]).Normalized();

    float Ex = std::abs(WorldRight.X) * (ScaledWidth * 0.5f) + std::abs(WorldUp.X) * (ScaledHeight * 0.5f);
    float Ey = std::abs(WorldRight.Y) * (ScaledWidth * 0.5f) + std::abs(WorldUp.Y) * (ScaledHeight * 0.5f);
    float Ez = std::abs(WorldRight.Z) * (ScaledWidth * 0.5f) + std::abs(WorldUp.Z) * (ScaledHeight * 0.5f);
    FVector Extent(Ex, Ey, Ez);

    FVector WorldCenter = GetWorldLocation();

    WorldAABB.Expand(WorldCenter - Extent);
    WorldAABB.Expand(WorldCenter + Extent);
}

bool UFireballComponent::RaycastMesh(const FRay& Ray, FHitResult& OutHitResult)
{
    // Fireball does not have a mesh by itself.
    return false;
}