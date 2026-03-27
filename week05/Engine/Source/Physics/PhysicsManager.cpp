#include "PhysicsManager.h"
#include "Scene/Scene.h"
#include "Math/Vector.h"
#include "Math/MathUtility.h"
#include "Actor/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/UUIDBillboardComponent.h"
#include "Component/TextComponent.h"

bool CPhysicsManager::Linetrace(const UScene* Scene, const FVector& Start, const FVector& End, FHitResult& OutHit)
{
	const TArray<AActor*>& Actors = Scene->GetActors();

	FVector LineDirection = End - Start;
	LineDirection.Normalize();

	// Normalize 실패 상황 고려
	if (!LineDirection.IsZero())
	{
		for (AActor* Actor : Actors)
		{
			if (!Actor || Actor->IsPendingDestroy())
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{

				if (!Component->IsA(UPrimitiveComponent::StaticClass()))
				{
					continue;
				}

				UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);
				if (!PrimitiveComponent->ShouldDrawDebugBounds()) continue;
				if (!PrimitiveComponent->GetPrimitive() || !PrimitiveComponent->GetPrimitive()->GetMeshData())
				{
					continue;
				}

				FBoxSphereBounds Bound = PrimitiveComponent->GetWorldBounds();

				FVector MaxBound = Bound.Center + Bound.BoxExtent;
				FVector MinBound = Bound.Center - Bound.BoxExtent;

				bool bStartInside =
					(MinBound.X <= Start.X && Start.X <= MaxBound.X) &&
					(MinBound.Y <= Start.Y && Start.Y <= MaxBound.Y) &&
					(MinBound.Z <= Start.Z && Start.Z <= MaxBound.Z);

				bool bEndInside =
					(MinBound.X <= End.X && End.X <= MaxBound.X) &&
					(MinBound.Y <= End.Y && End.Y <= MaxBound.Y) &&
					(MinBound.Z <= End.Z && End.Z <= MaxBound.Z);

				bool bIsLineInside = bStartInside || bEndInside;

				FVector VecToOrigin = Bound.Center - Start;
				float Length = (End - Start).Size();
				float ShortestT = FVector::DotProduct(VecToOrigin, LineDirection);
				// start 
				ShortestT = FMath::Clamp(ShortestT, 0.0f, Length);

				FVector ShortestPos = Start + LineDirection * ShortestT;

				if (bStartInside && bEndInside)
				{
					// Line 이 전부 다 AABB 안에 포함되는 경우 Shortest Pos 반환
					OutHit.HitActor = Actor;
					OutHit.HitLocation = ShortestPos;
					return true;
				}
				else
				{
					float ShortestDistSquared = (ShortestPos - Bound.Center).Size();

					// 빠른 검사를 위해 일차적으로 Sphere 로 test
					if (ShortestDistSquared <= Bound.Radius)
					{
						// 정밀한 검사를 위해 Box test
						FVector SlabMin = Bound.Center - Bound.BoxExtent;
						FVector SlabMax = Bound.Center + Bound.BoxExtent;

						FVector DirectionInv(1 / LineDirection.X, 1 / LineDirection.Y, 1 / LineDirection.Z);

						FVector T1 = FVector::Multiply((SlabMin - Start), DirectionInv);
						FVector T2 = FVector::Multiply((SlabMax - Start), DirectionInv);

						FVector TMinVec = FVector::Min(T1, T2);
						FVector TMaxVec = FVector::Max(T1, T2);

						float tNear = FMath::Max(FMath::Max(TMinVec.X, TMinVec.Y), TMinVec.Z);
						float tFar = FMath::Min(FMath::Min(TMaxVec.X, TMaxVec.Y), TMaxVec.Z);

						bool bIntersected =
							(tNear <= tFar) &&
							(tFar >= 0.0f) &&
							(tNear <= Length);

						if (bIntersected)
						{
							OutHit.HitActor = Actor;

							// 시작점이 포함되는 경우 (tNear >= 0.f), 끝점 쪽의 HitLocation 반환
							if (tNear >= 0.f)
								OutHit.HitLocation = Start + LineDirection * tNear;
							else
								OutHit.HitLocation = Start + LineDirection * tFar;

							return true;
						}
					}
					else
					{
						// rejct
					}
				}
			}
		}
	}

	return false;
}
