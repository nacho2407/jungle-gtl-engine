#include "Picker.h"

#include "Scene/Scene.h"
#include "Actor/Actor.h"
#include "Camera/Camera.h"
#include "Component/PrimitiveComponent.h"
#include "Primitive/PrimitiveBase.h"
#include "Renderer/PrimitiveVertex.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Component/UUIDBillboardComponent.h"
#include "Actor/SkySphereActor.h" 
#include <limits>

FRay CPicker::ScreenToRay(const CCamera* Camera, int32 ScreenX, int32 ScreenY, int32 ScreenWidth, int32 ScreenHeight) const
{
	if (!Camera || ScreenWidth <= 0 || ScreenHeight <= 0)
	{
		return { FVector::ZeroVector, FVector::ForwardVector };
	}

	const FMatrix ViewMatrix = Camera->GetViewMatrix();
	const FMatrix ProjMatrix = Camera->GetProjectionMatrix();
	const FMatrix ViewInverse = ViewMatrix.GetInverse();
	//Ndc convert missing center pixel lerp (0.5) Half-pixel offset added
	const float NdcX = (2.0f * (ScreenX+0.5f) / ScreenWidth) - 1.0f;
	const float NdcY = 1.0f - (2.0f * (ScreenY+0.5f) / ScreenHeight);

	if (Camera->IsOrthographic())
	{
		const float ViewRight = NdcX * (Camera->GetOrthoWidth() * 0.5f);
		const float ViewUp = NdcY * (Camera->GetOrthoHeight() * 0.5f);

		FVector RayOrigin;
		RayOrigin.X = ViewRight * ViewInverse.M[1][0] + ViewUp * ViewInverse.M[2][0] + ViewInverse.M[3][0];
		RayOrigin.Y = ViewRight * ViewInverse.M[1][1] + ViewUp * ViewInverse.M[2][1] + ViewInverse.M[3][1];
		RayOrigin.Z = ViewRight * ViewInverse.M[1][2] + ViewUp * ViewInverse.M[2][2] + ViewInverse.M[3][2];

		return { RayOrigin, Camera->GetForward() };
	}

	const float ViewForward = 1.0f;
	const float ViewRight = NdcX / ProjMatrix.M[1][0];
	const float ViewUp = NdcY / ProjMatrix.M[2][1];

	FVector RayDirectionWorld;
	RayDirectionWorld.X = ViewForward * ViewInverse.M[0][0] + ViewRight * ViewInverse.M[1][0] + ViewUp * ViewInverse.M[2][0];
	RayDirectionWorld.Y = ViewForward * ViewInverse.M[0][1] + ViewRight * ViewInverse.M[1][1] + ViewUp * ViewInverse.M[2][1];
	RayDirectionWorld.Z = ViewForward * ViewInverse.M[0][2] + ViewRight * ViewInverse.M[1][2] + ViewUp * ViewInverse.M[2][2];
	RayDirectionWorld = RayDirectionWorld.GetSafeNormal();

	FVector RayOrigin;
	RayOrigin.X = ViewInverse.M[3][0];
	RayOrigin.Y = ViewInverse.M[3][1];
	RayOrigin.Z = ViewInverse.M[3][2];

	return { RayOrigin, RayDirectionWorld };
}

bool CPicker::RayTriangleIntersect(const FRay& Ray,
								   const FVector& V0, const FVector& V1, const FVector& V2,
								   float& OutDistance) const
{
	constexpr float Epsilon = 1.e-6f;

	const FVector Edge1 = V1 - V0;
	const FVector Edge2 = V2 - V0;

	const FVector H = FVector::CrossProduct(Ray.Direction, Edge2);
	const float A = FVector::DotProduct(Edge1, H);

	// Render path와 동일하게 back-face는 picking 대상에서 제외한다.
	if (A <= Epsilon)
	{
		return false;
	}

	const float F = 1.0f / A;
	const FVector S = Ray.Origin - V0;
	const float U = F * FVector::DotProduct(S, H);
	if (U < 0.0f || U > 1.0f)
	{
		return false;
	}

	const FVector Q = FVector::CrossProduct(S, Edge1);
	const float V = F * FVector::DotProduct(Ray.Direction, Q);
	if (V < 0.0f || U + V > 1.0f)
	{
		return false;
	}

	const float T = F * FVector::DotProduct(Edge2, Q);
	if (T > Epsilon)
	{
		OutDistance = T;
		return true;
	}

	return false;
}

AActor* CPicker::PickActor(UScene* Scene, int32 ScreenX, int32 ScreenY,
						   int32 ScreenWidth, int32 ScreenHeight) const
{
	if (!Scene || !Scene->GetCamera())
	{
		return nullptr;
	}

	CCamera* Camera = Scene->GetCamera();
	const FRay Ray = ScreenToRay(Camera, ScreenX, ScreenY, ScreenWidth, ScreenHeight);

	AActor* ClosestActor = nullptr;
	float ClosestDistance = (std::numeric_limits<float>::max)();

	for (AActor* Actor : Scene->GetActors())
	{
		if (!Actor || Actor->IsPendingDestroy())
		{
			continue;
		}
		if (!Actor->IsVisible() )
			continue;
		if (Actor->IsA<ASkySphereActor>())
			continue;
		
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component->IsA(UPrimitiveComponent::StaticClass()))
			{
				continue;
			}
			if (Component->IsA(UUUIDBillboardComponent::StaticClass()))
			{
				continue;
			}
			UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);
			if (!PrimitiveComponent)
			{
				continue;
			}

			const bool bIsSubUV = PrimitiveComponent->IsA(USubUVComponent::StaticClass());
			const bool bIsText = PrimitiveComponent->IsA(UTextComponent::StaticClass());
			if (bIsSubUV || bIsText)
			{
				const FBoxSphereBounds Bounds = PrimitiveComponent->GetWorldBounds();

				FVector ToCenter = Bounds.Center - Ray.Origin;
				float T = FVector::DotProduct(ToCenter, Ray.Direction);
				if (T < 0.0f)
				{
					continue;
				}

				const FVector ClosestPoint = Ray.Origin + Ray.Direction * T;
				const float DistSq = (ClosestPoint - Bounds.Center).SizeSquared();
				const float RadiusSq = Bounds.Radius * Bounds.Radius;

				if (DistSq <= RadiusSq && T < ClosestDistance)
				{
					ClosestDistance = T;
					ClosestActor = Actor;
				}

				continue;
			}

			if (!PrimitiveComponent->GetPrimitive())
			{
				continue;
			}

			FMeshData* Mesh = PrimitiveComponent->GetPrimitive()->GetMeshData();
			if (!Mesh)
			{
				continue;
			}

			const FMatrix World = PrimitiveComponent->GetWorldTransform();

			for (uint32 Index = 0; Index + 2 < Mesh->Indices.size(); Index += 3)
			{
				const FVector& P0 = Mesh->Vertices[Mesh->Indices[Index]].Position;
				const FVector& P1 = Mesh->Vertices[Mesh->Indices[Index + 1]].Position;
				const FVector& P2 = Mesh->Vertices[Mesh->Indices[Index + 2]].Position;

				const FVector W0 = {
					P0.X * World.M[0][0] + P0.Y * World.M[1][0] + P0.Z * World.M[2][0] + World.M[3][0],
					P0.X * World.M[0][1] + P0.Y * World.M[1][1] + P0.Z * World.M[2][1] + World.M[3][1],
					P0.X * World.M[0][2] + P0.Y * World.M[1][2] + P0.Z * World.M[2][2] + World.M[3][2]
				};
				const FVector W1 = {
					P1.X * World.M[0][0] + P1.Y * World.M[1][0] + P1.Z * World.M[2][0] + World.M[3][0],
					P1.X * World.M[0][1] + P1.Y * World.M[1][1] + P1.Z * World.M[2][1] + World.M[3][1],
					P1.X * World.M[0][2] + P1.Y * World.M[1][2] + P1.Z * World.M[2][2] + World.M[3][2]
				};
				const FVector W2 = {
					P2.X * World.M[0][0] + P2.Y * World.M[1][0] + P2.Z * World.M[2][0] + World.M[3][0],
					P2.X * World.M[0][1] + P2.Y * World.M[1][1] + P2.Z * World.M[2][1] + World.M[3][1],
					P2.X * World.M[0][2] + P2.Y * World.M[1][2] + P2.Z * World.M[2][2] + World.M[3][2]
				};

				float Distance = 0.0f;
				if (RayTriangleIntersect(Ray, W0, W1, W2, Distance) && Distance < ClosestDistance)
				{
					ClosestDistance = Distance;
					ClosestActor = Actor;
				}
			}
		}
	}

	return ClosestActor;
}
