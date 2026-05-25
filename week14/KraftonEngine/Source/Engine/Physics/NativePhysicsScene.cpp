#include "Physics/NativePhysicsScene.h"
#include "Collision/Math/CollisionMath.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ShapeComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"

#include <algorithm>
#include <cmath>

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
	FBodyState& State = BodyStates[Comp];
	State.Mass = Comp->GetMass() > 0.0f ? Comp->GetMass() : 1.0f;
	State.CenterOfMassLocal = Comp->GetCenterOfMass();
}

void FNativePhysicsScene::RebuildBody(UPrimitiveComponent* Comp)
{
	auto It = BodyStates.find(Comp);
	if (It == BodyStates.end()) return; // 등록 안 됨 — skip
	// Native는 SimulatePhysics/ObjectType/Response를 매 Tick에서 컴포넌트로부터 직접 읽으므로
	// BodyState의 Mass/COM만 갱신.
	It->second.Mass = (Comp->GetMass() > 0.0f) ? Comp->GetMass() : 1.0f;
	It->second.CenterOfMassLocal = Comp->GetCenterOfMass();
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

	// ── 힘 적분 + 중력: bSimulatePhysics인 컴포넌트에 적용 ──
	for (UPrimitiveComponent* Comp : RegisteredComponents)
	{
		if (!Comp->GetSimulatePhysics()) continue;

		FBodyState& State = BodyStates[Comp];

		// 외부 힘/토크 적분
		float InvMass = (State.Mass > 0.0f) ? (1.0f / State.Mass) : 0.0f;
		State.Velocity = State.Velocity + State.AccumulatedForce * InvMass * DeltaTime;
		State.AngularVelocity = State.AngularVelocity + State.AccumulatedTorque * InvMass * DeltaTime;

		// 중력
		State.Velocity.Z += GravityZ * DeltaTime;

		FVector Pos = Comp->GetWorldLocation();
		Pos = Pos + State.Velocity * DeltaTime;
		Comp->SetWorldLocation(Pos);

		// 프레임 끝 누적값 초기화
		State.AccumulatedForce = { 0, 0, 0 };
		State.AccumulatedTorque = { 0, 0, 0 };
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

	// End Hit
	for (const FOverlapPair& Pair : PreviousBlockPairs)
	{
		if (CurrentBlockPairs.find(Pair) == CurrentBlockPairs.end())
		{
			Pair.A->NotifyComponentEndHit(Pair.A, Pair.B->GetOwner(), Pair.B);
			Pair.B->NotifyComponentEndHit(Pair.B, Pair.A->GetOwner(), Pair.A);
		}
	}

	PreviousOverlaps = CurrentOverlaps;
	PreviousBlockPairs = CurrentBlockPairs;
}

// ============================================================
// Force / Torque
// ============================================================

void FNativePhysicsScene::AddForce(UPrimitiveComponent* Comp, const FVector& Force)
{
	auto It = BodyStates.find(Comp);
	if (It == BodyStates.end()) return;
	It->second.AccumulatedForce = It->second.AccumulatedForce + Force;
}

void FNativePhysicsScene::AddForceAtLocation(UPrimitiveComponent* Comp, const FVector& Force, const FVector& WorldLocation)
{
	auto It = BodyStates.find(Comp);
	if (It == BodyStates.end()) return;

	It->second.AccumulatedForce = It->second.AccumulatedForce + Force;

	// COM = Comp의 world origin + 컴포넌트 회전 적용한 local offset
	FVector COMWorld = Comp->GetWorldLocation()
		+ Comp->GetWorldMatrix().ToQuat().RotateVector(It->second.CenterOfMassLocal);
	FVector R = WorldLocation - COMWorld;
	FVector Torque;
	Torque.X = R.Y * Force.Z - R.Z * Force.Y;
	Torque.Y = R.Z * Force.X - R.X * Force.Z;
	Torque.Z = R.X * Force.Y - R.Y * Force.X;
	It->second.AccumulatedTorque = It->second.AccumulatedTorque + Torque;
}

void FNativePhysicsScene::AddTorque(UPrimitiveComponent* Comp, const FVector& Torque)
{
	auto It = BodyStates.find(Comp);
	if (It == BodyStates.end()) return;
	It->second.AccumulatedTorque = It->second.AccumulatedTorque + Torque;
}

// ============================================================
// Velocity
// ============================================================

FVector FNativePhysicsScene::GetLinearVelocity(UPrimitiveComponent* Comp) const
{
	auto It = BodyStates.find(Comp);
	if (It == BodyStates.end()) return { 0, 0, 0 };
	return It->second.Velocity;
}

void FNativePhysicsScene::SetLinearVelocity(UPrimitiveComponent* Comp, const FVector& Vel)
{
	auto It = BodyStates.find(Comp);
	if (It == BodyStates.end()) return;
	It->second.Velocity = Vel;
}

FVector FNativePhysicsScene::GetAngularVelocity(UPrimitiveComponent* Comp) const
{
	auto It = BodyStates.find(Comp);
	if (It == BodyStates.end()) return { 0, 0, 0 };
	return It->second.AngularVelocity;
}

void FNativePhysicsScene::SetAngularVelocity(UPrimitiveComponent* Comp, const FVector& Vel)
{
	auto It = BodyStates.find(Comp);
	if (It == BodyStates.end()) return;
	It->second.AngularVelocity = Vel;
}

// ============================================================
// Mass
// ============================================================

void FNativePhysicsScene::SetMass(UPrimitiveComponent* Comp, float Mass)
{
	auto It = BodyStates.find(Comp);
	if (It == BodyStates.end()) return;
	It->second.Mass = (Mass > 0.0f) ? Mass : 1.0f;
}

float FNativePhysicsScene::GetMass(UPrimitiveComponent* Comp) const
{
	auto It = BodyStates.find(Comp);
	if (It == BodyStates.end()) return 1.0f;
	return It->second.Mass;
}

void FNativePhysicsScene::SetCenterOfMass(UPrimitiveComponent* Comp, const FVector& LocalOffset)
{
	auto It = BodyStates.find(Comp);
	if (It == BodyStates.end()) return;
	It->second.CenterOfMassLocal = LocalOffset;
}

FVector FNativePhysicsScene::GetCenterOfMass(UPrimitiveComponent* Comp) const
{
	auto It = BodyStates.find(Comp);
	if (It == BodyStates.end()) return { 0, 0, 0 };
	return It->second.CenterOfMassLocal;
}

// ============================================================
// Raycast — brute-force AABB ray test
// ============================================================

namespace
{
	// AABB slab 테스트 + closest-hit 갱신. 어떤 컴포넌트가 통과되는지 정하는 predicate 만
	// 호출자가 주입. Raycast / RaycastByObjectTypes 가 같은 기하 코드를 공유한다.
	template<typename FPredicate>
	bool NativeRaycastImpl(
		const std::vector<UPrimitiveComponent*>& RegisteredComponents,
		const FVector& Start, const FVector& Dir, float MaxDist,
		const AActor* IgnoreActor,
		FPredicate AcceptComponent,
		FHitResult& OutHit)
	{
		FVector InvDir;
		InvDir.X = (Dir.X != 0.0f) ? (1.0f / Dir.X) : 1e30f;
		InvDir.Y = (Dir.Y != 0.0f) ? (1.0f / Dir.Y) : 1e30f;
		InvDir.Z = (Dir.Z != 0.0f) ? (1.0f / Dir.Z) : 1e30f;

		float ClosestDist = MaxDist;
		bool bFound = false;

		for (UPrimitiveComponent* Comp : RegisteredComponents)
		{
			if (!Comp) continue;
			if (IgnoreActor && Comp->GetOwner() == IgnoreActor) continue;
			if (!AcceptComponent(Comp)) continue;

			FBoundingBox Box = Comp->GetWorldBoundingBox();

			float tMin = (Box.Min.X - Start.X) * InvDir.X;
			float tMax = (Box.Max.X - Start.X) * InvDir.X;
			if (tMin > tMax) { float tmp = tMin; tMin = tMax; tMax = tmp; }

			float tyMin = (Box.Min.Y - Start.Y) * InvDir.Y;
			float tyMax = (Box.Max.Y - Start.Y) * InvDir.Y;
			if (tyMin > tyMax) { float tmp = tyMin; tyMin = tyMax; tyMax = tmp; }

			if ((tMin > tyMax) || (tyMin > tMax)) continue;
			if (tyMin > tMin) tMin = tyMin;
			if (tyMax < tMax) tMax = tyMax;

			float tzMin = (Box.Min.Z - Start.Z) * InvDir.Z;
			float tzMax = (Box.Max.Z - Start.Z) * InvDir.Z;
			if (tzMin > tzMax) { float tmp = tzMin; tzMin = tzMax; tzMax = tmp; }

			if ((tMin > tzMax) || (tzMin > tMax)) continue;
			if (tzMin > tMin) tMin = tzMin;

			if (tMin < 0.0f) tMin = 0.0f;
			if (tMin >= ClosestDist) continue;

			ClosestDist = tMin;
			bFound = true;

			OutHit.bHit = true;
			OutHit.Distance = tMin;
			OutHit.HitComponent = Comp;
			OutHit.HitActor = Comp->GetOwner();
			OutHit.WorldHitLocation = Start + Dir * tMin;
		}

		return bFound;
	}
}

bool FNativePhysicsScene::Raycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
	ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
	// Channel filter: 응답이 TraceChannel에 대해 Block이 아니면 skip
	// (overlap/ignore 응답인 trigger volume 등은 raycast 결과에서 제외)
	return NativeRaycastImpl(RegisteredComponents, Start, Dir, MaxDist, IgnoreActor,
		[TraceChannel](UPrimitiveComponent* Comp) {
			return Comp->GetCollisionResponseToChannel(TraceChannel) == ECollisionResponse::Block;
		}, OutHit);
}

bool FNativePhysicsScene::RaycastByObjectTypes(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
	uint32 ObjectTypeMask, const AActor* IgnoreActor) const
{
	if (ObjectTypeMask == 0) return false;

	// ObjectType 자체를 마스크로 필터. 응답은 보지 않음.
	// Trigger 류 (NoCollision / query 비활성) 는 query 의미상 hit 후보가 아니므로 제외.
	return NativeRaycastImpl(RegisteredComponents, Start, Dir, MaxDist, IgnoreActor,
		[ObjectTypeMask](UPrimitiveComponent* Comp) {
			if (!Comp->IsQueryCollisionEnabled()) return false;
			const uint32 Bit = 1u << static_cast<uint32>(Comp->GetCollisionObjectType());
			return (Bit & ObjectTypeMask) != 0;
		}, OutHit);
}

bool FNativePhysicsScene::SphereSweepShapeComponents(const FVector& Start, const FVector& Dir, float MaxDist, float Radius,
	FHitResult& OutHit, ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
	if (MaxDist <= 0.0f || Radius < 0.0f)
	{
		return false;
	}

	float ClosestDist = MaxDist;
	bool bFound = false;

	for (UPrimitiveComponent* Comp : RegisteredComponents)
	{
		if (!Comp) continue;
		UShapeComponent* Shape = Cast<UShapeComponent>(Comp);
		if (!Shape) continue;
		if (IgnoreActor && Comp->GetOwner() == IgnoreActor) continue;
		if (Comp->GetCollisionResponseToChannel(TraceChannel) != ECollisionResponse::Block) continue;

		FHitResult CandidateHit;
		if (!FCollisionMath::SweepSphereShapeComponent(Start, Dir, MaxDist, Radius, Shape, CandidateHit))
		{
			continue;
		}
		if (CandidateHit.Distance >= ClosestDist)
		{
			continue;
		}

		ClosestDist = CandidateHit.Distance;
		bFound = true;
		OutHit = CandidateHit;
	}

	return bFound;
}
