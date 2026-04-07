#include "Picker.h"

#include "Actor/Actor.h"
#include "Camera/Camera.h"
#include "Component/PrimitiveComponent.h"
#include "Renderer/MeshData.h"
#include "Level/Level.h"
#include "Viewport/Viewport.h"

#include <algorithm>
#include "EditorEngine.h"
#include <cmath>
#include <limits>

namespace
{
	FVector TransformPointRowVector(const FVector& P, const FMatrix& M)
	{
		return {
			P.X * M.M[0][0] + P.Y * M.M[1][0] + P.Z * M.M[2][0] + M.M[3][0],
			P.X * M.M[0][1] + P.Y * M.M[1][1] + P.Z * M.M[2][1] + M.M[3][1],
			P.X * M.M[0][2] + P.Y * M.M[1][2] + P.Z * M.M[2][2] + M.M[3][2]
		};
	}

	FVector TransformVectorRowVector(const FVector& V, const FMatrix& M)
	{
		return {
			V.X * M.M[0][0] + V.Y * M.M[1][0] + V.Z * M.M[2][0],
			V.X * M.M[0][1] + V.Y * M.M[1][1] + V.Z * M.M[2][1],
			V.X * M.M[0][2] + V.Y * M.M[1][2] + V.Z * M.M[2][2]
		};
	}

	bool RayIntersectsSphere(const FRay& Ray, const FVector& Center, float Radius, float& OutT)
	{
		const FVector M = Ray.Origin - Center;
		const float B = FVector::DotProduct(M, Ray.Direction);
		const float C = FVector::DotProduct(M, M) - Radius * Radius;

		if (C > 0.0f && B > 0.0f)
		{
			return false;
		}

		const float Discriminant = B * B - C;
		if (Discriminant < 0.0f)
		{
			return false;
		}

		float T = -B - std::sqrt(Discriminant);
		if (T < 0.0f)
		{
			T = 0.0f;
		}

		OutT = T;
		return true;
	}

	bool RayIntersectsAABB(const FRay& Ray, const FVector& BoxMin, const FVector& BoxMax, float& OutTNear, float& OutTFar)
	{
		constexpr float Epsilon = 1.0e-8f;

		float TNear = 0.0f;
		float TFar = (std::numeric_limits<float>::max)();

		auto TestAxis = [&](float Origin, float Dir, float MinV, float MaxV) -> bool
			{
				if (std::abs(Dir) < Epsilon)
				{
					return (Origin >= MinV && Origin <= MaxV);
				}

				const float InvDir = 1.0f / Dir;
				float T1 = (MinV - Origin) * InvDir;
				float T2 = (MaxV - Origin) * InvDir;

				if (T1 > T2)
				{
					std::swap(T1, T2);
				}

				TNear = (std::max)(TNear, T1);
				TFar = (std::min)(TFar, T2);

				return TNear <= TFar;
			};

		if (!TestAxis(Ray.Origin.X, Ray.Direction.X, BoxMin.X, BoxMax.X)) return false;
		if (!TestAxis(Ray.Origin.Y, Ray.Direction.Y, BoxMin.Y, BoxMax.Y)) return false;
		if (!TestAxis(Ray.Origin.Z, Ray.Direction.Z, BoxMin.Z, BoxMax.Z)) return false;

		if (TFar < 0.0f)
		{
			return false;
		}

		OutTNear = TNear;
		OutTFar = TFar;
		return true;
	}
}

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
		case EViewportType::OrthoTop: Forward = FVector::DownVector; break;
		case EViewportType::OrthoBottom: Forward = FVector::UpVector; break;
		case EViewportType::OrthoLeft: Forward = FVector::RightVector; break;
		case EViewportType::OrthoRight: Forward = FVector::LeftVector; break;
		case EViewportType::OrthoFront: Forward = FVector::BackwardVector; break;
		case EViewportType::OrthoBack: Forward = FVector::ForwardVector; break;
		default: break;
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

AActor* FPicker::PickActor(ULevel* Scene, const FViewportEntry* Entry, int32 ScreenX, int32 ScreenY, FEditorEngine* Engine) const
{
	if (!Scene || !Entry)
	{
		return nullptr;
	}

	const FRay WorldRay = ScreenToRay(*Entry, ScreenX, ScreenY);

	AActor* ClosestActor = nullptr;
	float ClosestDistance = (std::numeric_limits<float>::max)();

	Scene->VisitPrimitivesByRay(
		WorldRay.Origin,
		WorldRay.Direction,
		ClosestDistance,
		[&](UPrimitiveComponent* PrimComp, float BoundsNear, float BoundsFar, float& InOutClosestDistance)
		{
			if (!PrimComp || PrimComp->IsPendingKill())
			{
				return;
			}

			AActor* Actor = PrimComp->GetOwner();
			if (!Actor || Actor->IsPendingDestroy() || !Actor->IsVisible())
			{
				return;
			}

			if (!PrimComp->IsPickable())
			{
				return;
			}

			if (PrimComp->UseSpherePicking())
			{
				const FBoxSphereBounds Bounds = PrimComp->GetWorldBounds();
				float SphereT = 0.0f;
				if (RayIntersectsSphere(WorldRay, Bounds.Center, Bounds.Radius, SphereT) && SphereT < InOutClosestDistance)
				{
					InOutClosestDistance = SphereT;
					ClosestActor = Actor;
				}
				return;
			}

			const float BoundsHitT = (BoundsNear >= 0.0f) ? BoundsNear : BoundsFar;
			if (BoundsHitT > InOutClosestDistance)
			{
				return;
			}

			if (PrimComp->HasMeshIntersection())
			{
				const FMatrix World = PrimComp->GetWorldTransform();
				const FMatrix InvWorld = World.GetInverse();

				FRay LocalRay;
				LocalRay.Origin = TransformPointRowVector(WorldRay.Origin, InvWorld);
				LocalRay.Direction = TransformVectorRowVector(WorldRay.Direction, InvWorld).GetSafeNormal();
				if (!LocalRay.Direction.IsZero())
				{
					float LocalDistance = (std::numeric_limits<float>::max)();
					if (PrimComp->IntersectLocalRay(LocalRay.Origin, LocalRay.Direction, LocalDistance))
					{
						const FVector LocalHitPoint = LocalRay.Origin + LocalRay.Direction * LocalDistance;
						const FVector WorldHitPoint = TransformPointRowVector(LocalHitPoint, World);
						const float WorldDistance = (WorldHitPoint - WorldRay.Origin).Size();

						if (WorldDistance < InOutClosestDistance)
						{
							InOutClosestDistance = WorldDistance;
							ClosestActor = Actor;
						}
					}
				}
				return;
			}

			if (BoundsHitT < InOutClosestDistance)
			{
				InOutClosestDistance = BoundsHitT;
				ClosestActor = Actor;
			}
		});
	return ClosestActor;
}