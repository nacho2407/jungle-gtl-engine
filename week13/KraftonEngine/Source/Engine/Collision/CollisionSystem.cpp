#include "Collision/CollisionSystem.h"
#include "Collision/CollisionMath.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"

#include <algorithm>

void FCollisionSystem::RegisterComponent(UPrimitiveComponent* Comp)
{
	if (!Comp) return;

	// 중복 등록 방지
	for (UPrimitiveComponent* Existing : RegisteredComponents)
	{
		if (Existing == Comp) return;
	}
	RegisteredComponents.push_back(Comp);
}

void FCollisionSystem::UnregisterComponent(UPrimitiveComponent* Comp)
{
	if (!Comp) return;

	// 등록 목록에서 제거
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

			// 제거되는 쪽에게 EndOverlap (이미 파괴 중이므로 안전하지 않을 수 있음 — 생략)
			// 남아있는 쪽에게 EndOverlap
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

	// CurrentOverlaps에서도 제거 (Tick 중간에 Unregister 될 수 있으므로)
	auto CurIt = CurrentOverlaps.begin();
	while (CurIt != CurrentOverlaps.end())
	{
		if (CurIt->A == Comp || CurIt->B == Comp)
		{
			CurIt = CurrentOverlaps.erase(CurIt);
		}
		else
		{
			++CurIt;
		}
	}
}

void FCollisionSystem::Tick(UWorld* World, float DeltaTime)
{
	if (!World) return;  

	CurrentOverlaps.clear();

	// Brute-force O(N^2) 쌍별 검사 — 등록된 컴포넌트만 대상
	const int32 Count = static_cast<int32>(RegisteredComponents.size());
	for (int32 i = 0; i < Count; ++i)
	{
		for (int32 j = i + 1; j < Count; ++j)
		{
			UPrimitiveComponent* A = RegisteredComponents[i];
			UPrimitiveComponent* B = RegisteredComponents[j];

			// 같은 액터의 컴포넌트끼리는 무시
			if (A->GetOwner() == B->GetOwner()) continue;

			// 채널 필터
			ECollisionResponse Resp = UPrimitiveComponent::GetMinResponse(A, B);
			if (Resp == ECollisionResponse::Ignore) continue;

			// Broad-phase: AABB 교차 먼저 확인
			FBoundingBox BoundsA = A->GetWorldBoundingBox();
			FBoundingBox BoundsB = B->GetWorldBoundingBox();
			if (!FCollisionMath::AABBvsAABB(BoundsA.Min, BoundsA.Max, BoundsB.Min, BoundsB.Max))
			{
				continue;
			}

			// Narrow-phase
			FHitResult Hit;
			if (!FCollisionMath::TestComponentPair(A, B, Hit))
			{
				continue;
			}

			// Response dispatch
			if (Resp == ECollisionResponse::Block)
			{
				// Hit 이벤트 — 양쪽 모두에게 통지
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
				// Overlap 쌍 기록 — Begin/End는 diff에서 처리
				if (A->GetGenerateOverlapEvents() || B->GetGenerateOverlapEvents())
				{
					CurrentOverlaps.insert(FOverlapPair{ A, B });
				}
			}
		}
	}

	// Diff: Begin/End Overlap 이벤트 발화

	// 새로 시작된 오버랩
	for (const FOverlapPair& Pair : CurrentOverlaps)
	{
		if (PreviousOverlaps.find(Pair) == PreviousOverlaps.end())
		{
			FHitResult DummyHit;

			if (Pair.A->GetGenerateOverlapEvents())
			{
				Pair.A->NotifyComponentBeginOverlap(
					Pair.A, Pair.B->GetOwner(), Pair.B, 0, false, DummyHit);
			}

			if (Pair.B->GetGenerateOverlapEvents())
			{
				Pair.B->NotifyComponentBeginOverlap(
					Pair.B, Pair.A->GetOwner(), Pair.A, 0, false, DummyHit);
			}
		}
	}

	// 끝난 오버랩
	for (const FOverlapPair& Pair : PreviousOverlaps)
	{
		if (CurrentOverlaps.find(Pair) == CurrentOverlaps.end())
		{
			if (Pair.A->GetGenerateOverlapEvents())
			{
				Pair.A->NotifyComponentEndOverlap(
					Pair.A, Pair.B->GetOwner(), Pair.B, 0);
			}

			if (Pair.B->GetGenerateOverlapEvents())
			{
				Pair.B->NotifyComponentEndOverlap(
					Pair.B, Pair.A->GetOwner(), Pair.A, 0);
			}
		}
	}

	// 프레임 교체
	PreviousOverlaps = CurrentOverlaps;
}
