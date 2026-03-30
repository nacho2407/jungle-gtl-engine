#include "Picker.h"

#include "Scene/Scene.h"
#include "Actor/Actor.h"
#include "Camera/Camera.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Component/UUIDBillboardComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Renderer/MeshData.h"
#include <limits>
#include "Component/SkyComponent.h"
#include "Viewport/Viewport.h"

FRay FPicker::ScreenToRay(const FViewportEntry& Entry, int32 ScreenX, int32 ScreenY) const
{
	if (!Entry.Viewport)
	{
		return { FVector::ZeroVector, FVector::ForwardVector };
	}

	const auto& Rect = Entry.Viewport->GetRect();
	if (Rect.Width <= 0 || Rect.Height <= 0)
	{
		return { FVector::ZeroVector, FVector::ForwardVector };
	}

	const float AspectRatio = static_cast<float>(Rect.Width) / static_cast<float>(Rect.Height);

	const FMatrix ViewMatrix = Entry.LocalState.BuildViewMatrix();
	const FMatrix ProjMatrix = Entry.LocalState.BuildProjMatrix(AspectRatio);
	const FMatrix ViewInverse = ViewMatrix.GetInverse();
	//Ndc convert missing center pixel lerp (0.5) Half-pixel offset added
	const float NdcX = (2.0f * (ScreenX + 0.5f) / Rect.Width) - 1.0f;
	const float NdcY = 1.0f - (2.0f * (ScreenY + 0.5f) / Rect.Height);

	if (Entry.LocalState.ProjectionType != EViewportType::Perspective)
	{
		const float ViewHeight = Entry.LocalState.OrthoZoom * 2.0f;
		const float ViewWidth = ViewHeight * AspectRatio;

		const float ViewRight = NdcX * (ViewWidth * 0.5f);
		const float ViewUp = NdcY * (ViewHeight * 0.5f);

		FVector RayOrigin;
		RayOrigin.X = ViewRight * ViewInverse.M[1][0] + ViewUp * ViewInverse.M[2][0] + ViewInverse.M[3][0];
		RayOrigin.Y = ViewRight * ViewInverse.M[1][1] + ViewUp * ViewInverse.M[2][1] + ViewInverse.M[3][1];
		RayOrigin.Z = ViewRight * ViewInverse.M[1][2] + ViewUp * ViewInverse.M[2][2] + ViewInverse.M[3][2];

		FVector Forward = FVector::ForwardVector;

		switch (Entry.LocalState.ProjectionType)
		{
		case EViewportType::OrthoTop:
			Forward = FVector::DownVector;
			break;

		case EViewportType::OrthoFront:
			Forward = FVector::BackwardVector;
			break;

		case EViewportType::OrthoRight:
			Forward = FVector::LeftVector;
			break;

		default:
			break;
		}

		return { RayOrigin, Forward };
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

bool FPicker::RayTriangleIntersect(const FRay& Ray,
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

AActor* FPicker::PickActor(UScene* Scene, const FViewportEntry* Entry, int32 ScreenX, int32 ScreenY) const
{
	if (!Entry)
	{
		return nullptr;
	}
	
	const FRay Ray = ScreenToRay(*Entry, ScreenX, ScreenY);

	AActor* ClosestActor = nullptr;
	float ClosestDistance = (std::numeric_limits<float>::max)();

	for (AActor* Actor : Scene->GetActors())
	{
		// 액터가 파괴 대기 중이거나 보이지 않으면 패스
		if (!Actor || Actor->IsPendingDestroy() || !Actor->IsVisible())
		{
			continue;
		}


		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (!Component || !Component->IsA(UPrimitiveComponent::StaticClass())) continue;

			// ─── 1. 피킹 제외 대상 (UUID 이름표, 하늘) ───
			if (Component->IsA(UUUIDBillboardComponent::StaticClass())) continue;
			if (Component->IsA(USkyComponent::StaticClass())) continue;

			UPrimitiveComponent* PrimComp = static_cast<UPrimitiveComponent*>(Component);

			// ─── 2. 바운딩 스피어(구형) 기반 피킹 (Text, SubUV) ───
			if (PrimComp->IsA(USubUVComponent::StaticClass()) || PrimComp->IsA(UTextComponent::StaticClass()))
			{
				const FBoxSphereBounds Bounds = PrimComp->GetWorldBounds();
				FVector ToCenter = Bounds.Center - Ray.Origin;
				float T = FVector::DotProduct(ToCenter, Ray.Direction);
				if (T < 0.0f) continue;

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

			// ─── 3. 정점 기반(폴리곤 단위) 정밀 피킹 (StaticMesh 등 일반 도형) ───
			if (PrimComp->IsA(UStaticMeshComponent::StaticClass()))
			{
				UStaticMeshComponent* SMC = static_cast<UStaticMeshComponent*>(PrimComp);
				FRenderMesh* Mesh = SMC->GetRenderMesh();

				// 메쉬가 없거나 정점이 비어있으면 패스
				if (!Mesh || Mesh->Vertices.empty() || Mesh->Indices.empty()) continue;

				const FMatrix World = SMC->GetWorldTransform();

				for (uint32 Index = 0; Index + 2 < Mesh->Indices.size(); Index += 3)
				{
					// ⭐ 이제 무조건 신형 FVertex 구조체를 쓰므로 코드가 하나로 통합됩니다.
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
	}

	return ClosestActor;
}