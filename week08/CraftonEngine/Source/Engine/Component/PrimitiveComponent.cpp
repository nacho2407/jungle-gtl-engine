#include "PrimitiveComponent.h"
#include "Object/ObjectFactory.h"
#include "Core/RayTypes.h"
#include "Collision/RayUtils.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Core/CollisionTypes.h"

DEFINE_CLASS(UPrimitiveComponent, USceneComponent)
IMPLEMENT_CLASS(UCubeComponent, UPrimitiveComponent)
IMPLEMENT_CLASS(USphereComponent, UPrimitiveComponent)
IMPLEMENT_CLASS(UPlaneComponent, UPrimitiveComponent)

void UPrimitiveComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Visible", EPropertyType::Bool, &bIsVisible });
}

void UPrimitiveComponent::UpdateWorldAABB() const
{
	FVector LExt = LocalExtents;

	FMatrix worldMatrix = GetWorldMatrix();

	float NewEx = std::abs(worldMatrix.M[0][0]) * LExt.X + std::abs(worldMatrix.M[1][0]) * LExt.Y + std::abs(worldMatrix.M[2][0]) * LExt.Z;
	float NewEy = std::abs(worldMatrix.M[0][1]) * LExt.X + std::abs(worldMatrix.M[1][1]) * LExt.Y + std::abs(worldMatrix.M[2][1]) * LExt.Z;
	float NewEz = std::abs(worldMatrix.M[0][2]) * LExt.X + std::abs(worldMatrix.M[1][2]) * LExt.Y + std::abs(worldMatrix.M[2][2]) * LExt.Z;

	FVector WorldCenter = GetWorldLocation();
	WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
	WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);
}

bool UPrimitiveComponent::LineTraceComponent(const FRay& Ray, FHitResult& OutHitResult)
{
	const FMeshData& Data = FMeshBufferManager::Get().GetMeshData(GetPrimitiveType());
	if (Data.Indices.empty()) return false;

	bool bHit = FRayUtils::RaycastTriangles(
		Ray, CachedWorldMatrix,
		&Data.Vertices[0].Position,
		sizeof(FVertex),
		Data.Indices,
		OutHitResult);

	if (bHit)
	{
		OutHitResult.HitComponent = this;
	}
	return bHit;
}

void UPrimitiveComponent::UpdateWorldMatrix() const
{
	USceneComponent::UpdateWorldMatrix();
	UpdateWorldAABB();
}

void UPrimitiveComponent::AddWorldOffset(const FVector& WorldDelta)
{
	USceneComponent::AddWorldOffset(WorldDelta);
}


UCubeComponent::UCubeComponent()
{
}

USphereComponent::USphereComponent()
{
}

void USphereComponent::UpdateWorldAABB() const
{
	FVector Center = { CachedWorldMatrix.M[3][0], CachedWorldMatrix.M[3][1], CachedWorldMatrix.M[3][2] };

	float ExtentX = LocalExtents.X * sqrtf(powf(CachedWorldMatrix.M[0][0], 2) +
		powf(CachedWorldMatrix.M[1][0], 2) +
		powf(CachedWorldMatrix.M[2][0], 2));

	float ExtentY = LocalExtents.Y * sqrtf(powf(CachedWorldMatrix.M[0][1], 2) +
		powf(CachedWorldMatrix.M[1][1], 2) +
		powf(CachedWorldMatrix.M[2][1], 2));

	float ExtentZ = LocalExtents.Z * sqrtf(powf(CachedWorldMatrix.M[0][2], 2) +
		powf(CachedWorldMatrix.M[1][2], 2) +
		powf(CachedWorldMatrix.M[2][2], 2));

	WorldAABBMinLocation = { Center.X - ExtentX, Center.Y - ExtentY, Center.Z - ExtentZ };
	WorldAABBMaxLocation = { Center.X + ExtentX, Center.Y + ExtentY, Center.Z + ExtentZ };
}

UPlaneComponent::UPlaneComponent()
{
}

void UPlaneComponent::UpdateWorldAABB() const
{
	// Plane mesh: XY 평면, Z 두께 ±0.01f
	FVector LExt = { 0.5f, 0.5f, 0.01f };

	float NewEx = std::abs(CachedWorldMatrix.M[0][0]) * LExt.X + std::abs(CachedWorldMatrix.M[1][0]) * LExt.Y + std::abs(CachedWorldMatrix.M[2][0]) * LExt.Z;
	float NewEy = std::abs(CachedWorldMatrix.M[0][1]) * LExt.X + std::abs(CachedWorldMatrix.M[1][1]) * LExt.Y + std::abs(CachedWorldMatrix.M[2][1]) * LExt.Z;
	float NewEz = std::abs(CachedWorldMatrix.M[0][2]) * LExt.X + std::abs(CachedWorldMatrix.M[1][2]) * LExt.Y + std::abs(CachedWorldMatrix.M[2][2]) * LExt.Z;

	FVector WorldCenter = GetWorldLocation();
	WorldAABBMinLocation = WorldCenter - FVector(NewEx, NewEy, NewEz);
	WorldAABBMaxLocation = WorldCenter + FVector(NewEx, NewEy, NewEz);
}

void UPlaneComponent::SetRelativeScale(const FVector& NewScale)
{
	FVector planeScale = FVector(NewScale.X, NewScale.Y, 1.0f);
	UPrimitiveComponent::SetRelativeScale(planeScale);
}