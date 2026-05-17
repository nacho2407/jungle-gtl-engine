#include "FireballComponent.h"

// 화면에서 컬링되지 않도록 수정한다.
UFireballComponent::UFireballComponent()
{
	bEnableCull = false;
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