#include "AnimNotifyState_AttackHitWindow.h"

#include "Component/Input/ActionComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Core/Types/CollisionTypes.h"
#include "Core/Types/EngineTypes.h"
#include "Debug/DrawDebugHelpers.h"
#include "Core/Logging/Log.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"

#include <cfloat>

namespace
{
	int32 FindBoneIndex(USkeletalMeshComponent* MeshComp, const FString& BoneName)
	{
		if (!MeshComp || BoneName.empty()) return -1;

		USkeletalMesh* Mesh = MeshComp->GetSkeletalMesh();
		FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		if (!Asset) return -1;

		for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Asset->Bones.size()); ++BoneIndex)
		{
			if (Asset->Bones[BoneIndex].Name == BoneName)
			{
				return BoneIndex;
			}
		}

		return -1;
	}

	FVector MakeActorLocalOffset(AActor* Actor, const FVector& LocalOffset)
	{
		if (!Actor) return LocalOffset;

		return Actor->GetActorForward() * LocalOffset.X
			+ Actor->GetActorRight() * LocalOffset.Y
			+ FVector::UpVector * LocalOffset.Z;
	}

	FVector GetHitCenter(USkeletalMeshComponent* MeshComp, AActor* Owner, const FString& BoneName, const FVector& LocalOffset)
	{
		const FVector WorldOffset = MakeActorLocalOffset(Owner, LocalOffset);
		const int32 BoneIndex = FindBoneIndex(MeshComp, BoneName);
		if (BoneIndex >= 0)
		{
			return MeshComp->GetBoneLocationByIndex(BoneIndex) + WorldOffset;
		}

		return Owner ? Owner->GetActorLocation() + WorldOffset : WorldOffset;
	}

	float DistanceSquaredPointAABB(const FVector& Point, const FBoundingBox& Box)
	{
		const float X = Point.X < Box.Min.X ? Box.Min.X - Point.X : (Point.X > Box.Max.X ? Point.X - Box.Max.X : 0.0f);
		const float Y = Point.Y < Box.Min.Y ? Box.Min.Y - Point.Y : (Point.Y > Box.Max.Y ? Point.Y - Box.Max.Y : 0.0f);
		const float Z = Point.Z < Box.Min.Z ? Box.Min.Z - Point.Z : (Point.Z > Box.Max.Z ? Point.Z - Box.Max.Z : 0.0f);
		return X * X + Y * Y + Z * Z;
	}

	void DrawDebugBounds(UWorld* World, const FBoundingBox& Bounds, const FColor& Color, float Duration)
	{
		if (!World || !Bounds.IsValid())
		{
			return;
		}

		DrawDebugBox(World, Bounds.GetCenter(), Bounds.GetExtent(), Color, Duration);
	}

	UActionComponent* GetOrCreateActionComponent(AActor* Actor, bool bAutoAdd)
	{
		if (!Actor) return nullptr;

		if (UActionComponent* Existing = Actor->GetComponentByClass<UActionComponent>())
		{
			return Existing;
		}

		return bAutoAdd ? Actor->AddComponent<UActionComponent>() : nullptr;
	}

	void ApplyHitStop(AActor* Actor, float Duration, bool bAutoAddActionComponent)
	{
		if (UActionComponent* Action = GetOrCreateActionComponent(Actor, bAutoAddActionComponent))
		{
			Action->LocalHitStop(Duration);
		}
	}

	FVector ResolveKnockbackDirection(AActor* Attacker, AActor* Target, EAttackKnockbackMode Mode)
	{
		switch (Mode)
		{
		case EAttackKnockbackMode::Up:
			return FVector::UpVector;
		case EAttackKnockbackMode::AwayFromAttacker:
		{
			if (!Attacker || !Target) return FVector::ForwardVector;
			FVector Delta = Target->GetActorLocation() - Attacker->GetActorLocation();
			Delta.Z = 0.0f; // 수평 성분만 — 높낮이 차이로 위/아래로 날아가는 일 방지.
			if (Delta.IsNearlyZero()) return Attacker->GetActorForward();
			return Delta.Normalized();
		}
		case EAttackKnockbackMode::Forward:
		default:
			return Attacker ? Attacker->GetActorForward() : FVector::ForwardVector;
		}
	}

	void ApplyKnockback(AActor* Attacker, AActor* Target, EAttackKnockbackMode Mode,
		float Distance, float Duration, bool bAutoAddActionComponent)
	{
		if (Distance <= 0.0f || !Target) return;

		UActionComponent* Action = GetOrCreateActionComponent(Target, bAutoAddActionComponent);
		if (!Action) return;

		const FVector Dir = ResolveKnockbackDirection(Attacker, Target, Mode);
		Action->Knockback(Dir, Distance, Duration);
	}

	UParticleSystemComponent* FindTrailParticleComponent(USkeletalMeshComponent* MeshComp, const FString& TrailActorTag)
	{
		if (!MeshComp)
		{
			return nullptr;
		}

		AActor* Owner = MeshComp->GetOwner();
		if (!Owner)
		{
			return nullptr;
		}

		if (UParticleSystemComponent* OwnerParticle = Owner->GetComponentByClass<UParticleSystemComponent>())
		{
			return OwnerParticle;
		}

		UWorld* World = MeshComp->GetWorld();
		if (!World || TrailActorTag.empty())
		{
			return nullptr;
		}

		const FName TagName(TrailActorTag);
		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || Actor == Owner || !Actor->HasTag(TagName))
			{
				continue;
			}

			if (UParticleSystemComponent* Particle = Actor->GetComponentByClass<UParticleSystemComponent>())
			{
				return Particle;
			}
		}

		return nullptr;
	}
}

void UAnimNotifyState_AttackHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/, float /*TotalDuration*/)
{
	if (!MeshComp)
	{
		return;
	}

	if (bControlTrailParticle)
	{
		if (UParticleSystemComponent* Trail = FindTrailParticleComponent(MeshComp, TrailActorTag))
		{
			if (bResetTrailOnBegin)
			{
				Trail->ResetSystem();
			}

			Trail->Activate();
			Trail->SetEmitterSpawningEnabled(true);
		}
	}

	HitActorsByMesh[MeshComp].clear();
	MissLoggedActorsByMesh[MeshComp].clear();
	NoTargetLoggedMeshes.erase(MeshComp);
}

void UAnimNotifyState_AttackHitWindow::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/, float /*FrameDeltaTime*/)
{
	if (!MeshComp || Radius <= 0.0f)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	UWorld* World = MeshComp->GetWorld();
	if (!Owner || !World)
	{
		return;
	}

	TSet<AActor*>& HitActors = HitActorsByMesh[MeshComp];
	TSet<AActor*>& MissLoggedActors = MissLoggedActorsByMesh[MeshComp];
	const FVector Center = GetHitCenter(MeshComp, Owner, BoneName, LocalOffset);
	if (bDrawDebugHitWindow)
	{
		DrawDebugSphere(World, Center, Radius, DebugDrawSegments, FColor(255, 220, 0), DebugDrawDuration);
	}

	bool bSawTargetCandidate = false;
	for (AActor* Candidate : World->GetActors())
	{
		if (!Candidate || Candidate == Owner)
		{
			continue;
		}

		const bool bMatchesTargetActorTag = !TargetActorTag.empty() && Candidate->HasTag(FName(TargetActorTag));
		if (bRequireTargetActorTag)
		{
			if (!bMatchesTargetActorTag)
			{
				continue;
			}
		}
		else if (!TargetActorTag.empty() && !bMatchesTargetActorTag)
		{
			continue;
		}

		bSawTargetCandidate = true;
		if (HitActors.find(Candidate) != HitActors.end())
		{
			continue;
		}

		UPrimitiveComponent* HitComponent = nullptr;
		UPrimitiveComponent* ClosestComponent = nullptr;
		const char* MissReason = "no primitive components";
		float ClosestDistanceSquared = FLT_MAX;
		for (UPrimitiveComponent* Primitive : Candidate->GetPrimitiveComponents())
		{
			if (!Primitive)
			{
				continue;
			}

			const FBoundingBox Bounds = Primitive->GetWorldBoundingBox();
			if (bRequireQueryCollision && !Primitive->IsQueryCollisionEnabled())
			{
				if (bDrawDebugTargetBounds)
				{
					DrawDebugBounds(World, Bounds, FColor(90, 90, 90), DebugDrawDuration);
				}
				MissReason = "query collision disabled";
				continue;
			}

			if (!bHitWorldStatic && !bMatchesTargetActorTag && Primitive->GetCollisionObjectType() == ECollisionChannel::WorldStatic)
			{
				if (bDrawDebugTargetBounds)
				{
					DrawDebugBounds(World, Bounds, FColor(80, 80, 160), DebugDrawDuration);
				}
				MissReason = "world static filtered";
				continue;
			}

			if (!Bounds.IsValid())
			{
				MissReason = "invalid bounds";
				continue;
			}

			const float DistanceSquared = DistanceSquaredPointAABB(Center, Bounds);
			const bool bIntersects = DistanceSquared <= Radius * Radius;
			if (bDrawDebugTargetBounds)
			{
				DrawDebugBounds(World, Bounds, bIntersects ? FColor(255, 40, 40) : FColor(0, 180, 255), DebugDrawDuration);
			}

			if (DistanceSquared < ClosestDistanceSquared)
			{
				ClosestDistanceSquared = DistanceSquared;
				ClosestComponent = Primitive;
				MissReason = "outside radius";
			}

			if (bIntersects)
			{
				HitComponent = Primitive;
				break;
			}
		}

		if (!HitComponent)
		{
			if (bLogMisses && MissLoggedActors.find(Candidate) == MissLoggedActors.end())
			{
				MissLoggedActors.insert(Candidate);
				UE_LOG("[AttackHitWindow] miss %s -> %s (%s%s%s center=%.1f, %.1f, %.1f radius=%.1f)",
					Owner->GetName().c_str(),
					Candidate->GetName().c_str(),
					MissReason,
					ClosestComponent ? " closest=" : "",
					ClosestComponent ? ClosestComponent->GetName().c_str() : "",
					Center.X,
					Center.Y,
					Center.Z,
					Radius);
			}
			continue;
		}

		HitActors.insert(Candidate);
		ApplyHitStop(Owner, HitStopDuration, bAutoAddActionComponent);
		ApplyHitStop(Candidate, HitStopDuration, bAutoAddActionComponent);
		if (bApplyKnockback)
		{
			ApplyKnockback(Owner, Candidate, KnockbackMode, KnockbackDistance, KnockbackDuration, bAutoAddActionComponent);
		}
		if (bDrawDebugHitWindow)
		{
			DrawDebugSphere(World, Center, Radius, DebugDrawSegments, FColor(255, 40, 40), DebugDrawDuration);
		}

		if (bLogHits)
		{
			UE_LOG("[AttackHitWindow] %s hit %s via %s (center=%.1f, %.1f, %.1f radius=%.1f)",
				Owner->GetName().c_str(),
				Candidate->GetName().c_str(),
				HitComponent->GetName().c_str(),
				Center.X,
				Center.Y,
				Center.Z,
				Radius);
		}
	}

	if (bLogMisses && !bSawTargetCandidate && NoTargetLoggedMeshes.find(MeshComp) == NoTargetLoggedMeshes.end())
	{
		NoTargetLoggedMeshes.insert(MeshComp);
		UE_LOG("[AttackHitWindow] no target candidate for %s (RequireTargetTag=%d TargetActorTag=%s)",
			Owner->GetName().c_str(),
			bRequireTargetActorTag ? 1 : 0,
			TargetActorTag.c_str());
	}
}

void UAnimNotifyState_AttackHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* /*Anim*/)
{
	if (bControlTrailParticle)
	{
		if (UParticleSystemComponent* Trail = FindTrailParticleComponent(MeshComp, TrailActorTag))
		{
			Trail->SetEmitterSpawningEnabled(false);
		}
	}

	HitActorsByMesh.erase(MeshComp);
	MissLoggedActorsByMesh.erase(MeshComp);
	NoTargetLoggedMeshes.erase(MeshComp);
}
