#pragma once

#include "Core/Types/CoreTypes.h"
#include "Physics/BodyInstance.h"

class FPhysicsScene;
class USkeletalMeshComponent;
class UPhysicsAsset;

// FHitboxInstance — PhysicsAsset 의 본별 BodySetup 으로 query-only kinematic body 를
// 만들어 raycast/overlap 의 정밀 hitbox 로 사용한다. Ragdoll 과 달리 시뮬레이션 X,
// 매 틱 SyncBodiesFromBones 로 애니메이션 본 transform 을 body 에 복사한다.
struct FHitboxInstance
{
	TArray<FBodyInstance> Bodies;
	TArray<int32> BodyToBoneIndex;
	FVector ComponentWorldScaleAtStart = FVector::OneVector;

	bool bInitialized = false;
	bool IsActive() const { return bInitialized; }
	uint32 GetBodyCount() const { return static_cast<uint32>(Bodies.size()); }

	void Initialize(UPhysicsAsset* Asset, USkeletalMeshComponent* MeshComp, FPhysicsScene* Scene);
	void Release(FPhysicsScene* Scene);

	// 매 틱 호출 — 현재 본 world transform 을 각 body 에 push (kinematic target).
	void SyncBodiesFromBones(USkeletalMeshComponent* MeshComp);
};
