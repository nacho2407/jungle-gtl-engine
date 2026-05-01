#include "Physics/NativePhysicsScene.h"
#include "Collision/CollisionMath.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"

#include <algorithm>

void FNativePhysicsScene::Initialize(UWorld* InWorld)
{
	World = InWorld;
}

void FNativePhysicsScene::Shutdown()
{
	RegisteredComponents.clear();
	PreviousOverlaps.clear();
	CurrentOverlaps.clear();
	World = nullptr;
}

void FNativePhysicsScene::RegisterComponent(UPrimitiveComponent* Comp)
{
	if (!Comp) return;

	for (UPrimitiveComponent* Existing : RegisteredComponents)
	{
		if (Existing == Comp) return;
	}
	RegisteredComponents.push_back(Comp);
}

void FNativePhysicsScene::UnregisterComponent(UPrimitiveComponent* Comp)
{
	if (!Comp) return;

	auto It = std::find(RegisteredComponents.begin(), RegisteredComponents.end(), Comp);
	if (It == RegisteredComponents.end()) return;
	RegisteredComponents.erase(It);

	// PreviousOverlaps에서 이 컴포넌트를 포함하는 쌍 제거 + EndOverlap 발화
	auto PairIt = PreviousOverlaps.begin();
	while (PairIt != PreviousOverlaps.end())
	{
		if (PairIt->A == Comp || PairIt->B == Comp)
		{
			UPrimitiveComponent* Other = (PairIt->A == Comp) ? PairIt->B : PairIt->A;

			if (Other->GetGenerateOverlapEvents())
			{
				AActor* CompOwner = Comp->GetOwner();
				Other->NotifyComponentEndOverlap(Other, CompOwner, Comp, 0);
			}

			PairIt = PreviousOverlaps.erase(PairIt);
		}
		else
		{
			++PairIt;
		}
	}

	// CurrentOverlaps에서도 제거
	auto CurIt = CurrentOverlaps.begin();
	while (CurIt != CurrentOverlaps.end())
	{
		if (CurIt->A == Comp || CurIt->B == Comp)
			CurIt = CurrentOverlaps.erase(CurIt);
		else
			++CurIt;
	}
}

void FNativePhysicsScene::Tick(float DeltaTime)
{
	if (!World) return;

	CurrentOverlaps.clear();

	// Brute-force O(N²)
	const int32 Count = static_cast<int32>(RegisteredComponents.size());
	for (int32 i = 0; i < Count; ++i)
	{
		for (int32 j = i + 1; j < Count; ++j)
		{
			UPrimitiveComponent* A = RegisteredComponents[i];
			UPrimitiveComponent* B = RegisteredComponents[j];

			if (A->GetOwner() == B->GetOwner()) continue;

			ECollisionResponse Resp = UPrimitiveComponent::GetMinResponse(A, B);
			if (Resp == ECollisionResponse::Ignore) continue;

			// Broad-phase: AABB
			FBoundingBox BoundsA = A->GetWorldBoundingBox();
			FBoundingBox BoundsB = B->GetWorldBoundingBox();
			if (!FCollisionMath::AABBvsAABB(BoundsA.Min, BoundsA.Max, BoundsB.Min, BoundsB.Max))
				continue;

			// Narrow-phase
			FHitResult Hit;
			if (!FCollisionMath::TestComponentPair(A, B, Hit))
				continue;

			if (Resp == ECollisionResponse::Block)
			{
				FVector NormalImpulse = Hit.ImpactNormal * Hit.PenetrationDepth;

				FHitResult HitA = Hit;
				HitA.HitComponent = B;
				HitA.HitActor = B->GetOwner();
				A->NotifyComponentHit(A, B->GetOwner(), B, NormalImpulse, HitA);

				FHitResult HitB = Hit;
				HitB.HitComponent = A;
				HitB.HitActor = A->GetOwner();
				HitB.ImpactNormal = Hit.ImpactNormal * -1.0f;
				HitB.WorldNormal = Hit.WorldNormal * -1.0f;
				B->NotifyComponentHit(B, A->GetOwner(), A, NormalImpulse * -1.0f, HitB);
			}
			else if (Resp == ECollisionResponse::Overlap)
			{
				if (A->GetGenerateOverlapEvents() || B->GetGenerateOverlapEvents())
				{
					CurrentOverlaps.insert(FOverlapPair{ A, B });
				}
			}
		}
	}

	// Begin Overlap
	for (const FOverlapPair& Pair : CurrentOverlaps)
	{
		if (PreviousOverlaps.find(Pair) == PreviousOverlaps.end())
		{
			FHitResult DummyHit;

			if (Pair.A->GetGenerateOverlapEvents())
				Pair.A->NotifyComponentBeginOverlap(Pair.A, Pair.B->GetOwner(), Pair.B, 0, false, DummyHit);

			if (Pair.B->GetGenerateOverlapEvents())
				Pair.B->NotifyComponentBeginOverlap(Pair.B, Pair.A->GetOwner(), Pair.A, 0, false, DummyHit);
		}
	}

	// End Overlap
	for (const FOverlapPair& Pair : PreviousOverlaps)
	{
		if (CurrentOverlaps.find(Pair) == CurrentOverlaps.end())
		{
			if (Pair.A->GetGenerateOverlapEvents())
				Pair.A->NotifyComponentEndOverlap(Pair.A, Pair.B->GetOwner(), Pair.B, 0);

			if (Pair.B->GetGenerateOverlapEvents())
				Pair.B->NotifyComponentEndOverlap(Pair.B, Pair.A->GetOwner(), Pair.A, 0);
		}
	}

	PreviousOverlaps = CurrentOverlaps;
}
