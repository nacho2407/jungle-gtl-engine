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
	BodyStates.clear();
	PreviousOverlaps.clear();
	CurrentOverlaps.clear();
	PreviousBlockPairs.clear();
	CurrentBlockPairs.clear();
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
	BodyStates[Comp] = {};
}

void FNativePhysicsScene::UnregisterComponent(UPrimitiveComponent* Comp)
{
	if (!Comp) return;

	auto It = std::find(RegisteredComponents.begin(), RegisteredComponents.end(), Comp);
	if (It == RegisteredComponents.end()) return;
	RegisteredComponents.erase(It);
	BodyStates.erase(Comp);

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

	// BlockPairs에서도 제거
	auto EraseFromSet = [Comp](std::unordered_set<FOverlapPair>& Set) {
		auto It = Set.begin();
		while (It != Set.end())
		{
			if (It->A == Comp || It->B == Comp)
				It = Set.erase(It);
			else
				++It;
		}
	};
	EraseFromSet(PreviousBlockPairs);
	EraseFromSet(CurrentBlockPairs);
}

void FNativePhysicsScene::Tick(float DeltaTime)
{
	if (!World) return;

	// ── 중력 적분: bSimulatePhysics인 컴포넌트에 중력 적용 ──
	for (UPrimitiveComponent* Comp : RegisteredComponents)
	{
		if (!Comp->GetSimulatePhysics()) continue;

		FBodyState& State = BodyStates[Comp];
		State.Velocity.Z += GravityZ * DeltaTime;

		FVector Pos = Comp->GetWorldLocation();
		Pos = Pos + State.Velocity * DeltaTime;
		Comp->SetWorldLocation(Pos);
	}

	CurrentOverlaps.clear();
	CurrentBlockPairs.clear();

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
				CurrentBlockPairs.insert(FOverlapPair{ A, B });

				// Hit 이벤트는 첫 접촉 시에만 발화 (PhysX eNOTIFY_TOUCH_FOUND 방식)
				if (PreviousBlockPairs.find(FOverlapPair{ A, B }) == PreviousBlockPairs.end())
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

				// ── Block 위치 보정 + 속도 보정 ──
				// TestComponentPair 내부에서 A/B가 swap될 수 있음.
				// Hit.ImpactNormal은 내부 A→B 방향. Hit.HitComponent가 내부 B.
				// 따라서 Hit.HitComponent == (우리의)B 면 Normal은 A→B — A를 밀 방향은 반대.
				//         Hit.HitComponent == (우리의)A 면 swap됐으므로 Normal은 B→A — A를 밀 방향은 그대로.
				bool bASimulates = A->GetSimulatePhysics();
				bool bBSimulates = B->GetSimulatePhysics();
				FVector PushA; // A를 밀어내는 방향 (B에서 멀어지는 방향)
				if (Hit.HitComponent == B)
					PushA = Hit.ImpactNormal * -1.0f;
				else
					PushA = Hit.ImpactNormal;
				FVector Normal = PushA;
				float Depth = Hit.PenetrationDepth;

				// Baumgarte stabilization + slop
				constexpr float Slop = 0.01f;
				constexpr float BaumgarteBeta = 0.2f;
				float CorrectionDepth = (std::max)(0.0f, Depth - Slop);

				if (CorrectionDepth > 0.0f)
				{
					float BiasVelocity = (BaumgarteBeta / DeltaTime) * CorrectionDepth;

					if (bASimulates && bBSimulates)
					{
						FVector Correction = Normal * (BiasVelocity * DeltaTime * 0.5f);
						A->SetWorldLocation(A->GetWorldLocation() + Correction);
						B->SetWorldLocation(B->GetWorldLocation() - Correction);
					}
					else if (bASimulates)
					{
						A->SetWorldLocation(A->GetWorldLocation() + Normal * (BiasVelocity * DeltaTime));
					}
					else if (bBSimulates)
					{
						B->SetWorldLocation(B->GetWorldLocation() - Normal * (BiasVelocity * DeltaTime));
					}
				}

				// 속도 반사: (1 + e) 로 반발계수 적용 (e=0 완전비탄성, e=1 완전탄성)
				constexpr float Restitution = 0.2f;
				if (bASimulates)
				{
					FBodyState& StateA = BodyStates[A];
					float VdotN = StateA.Velocity.X * Normal.X + StateA.Velocity.Y * Normal.Y + StateA.Velocity.Z * Normal.Z;
					if (VdotN < 0.0f)
						StateA.Velocity = StateA.Velocity - Normal * (VdotN * (1.0f + Restitution));
				}
				if (bBSimulates)
				{
					FBodyState& StateB = BodyStates[B];
					FVector NegNormal = Normal * -1.0f;
					float VdotN = StateB.Velocity.X * NegNormal.X + StateB.Velocity.Y * NegNormal.Y + StateB.Velocity.Z * NegNormal.Z;
					if (VdotN < 0.0f)
						StateB.Velocity = StateB.Velocity - NegNormal * (VdotN * (1.0f + Restitution));
				}
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
	PreviousBlockPairs = CurrentBlockPairs;
}
