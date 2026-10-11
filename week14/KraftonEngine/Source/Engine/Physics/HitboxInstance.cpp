#include "HitboxInstance.h"

#include "Physics/PhysicsAsset.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsScene.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Types/CollisionTypes.h"
#include "Math/Transform.h"

void FHitboxInstance::Initialize(UPhysicsAsset* Asset, USkeletalMeshComponent* MeshComp, FPhysicsScene* Scene)
{
	if (bInitialized || !Asset || !MeshComp || !Scene) return;

	ComponentWorldScaleAtStart = MeshComp->GetWorldScale();

	const TArray<UBodySetup*>& BodySetups = Asset->GetBodySetups();
	Bodies.reserve(BodySetups.size());
	BodyToBoneIndex.reserve(BodySetups.size());

	for (UBodySetup* Setup : BodySetups)
	{
		if (!Setup) continue;
		if (!Asset->IsBodyCollisionEnabled(Setup->GetBoneName())) continue;

		const FString BoneName = Setup->GetBoneName().ToString();
		FTransform BoneWorld;
		if (!MeshComp->GetBoneWorldTransformByName(BoneName, BoneWorld)) continue;

		Bodies.emplace_back();
		FBodyInstance& Body = Bodies.back();
		Body.bSyncOwnerFromPhysics = false;

		// bSimulatePhysics=true → PxRigidDynamic 으로 생성된 뒤, 바로 kinematic 으로 전환.
		// CollisionEnabled=QueryOnly 라 시뮬에서 무시되고 raycast 만 잡힘.
		const bool bCreated = Scene->CreateBodyFromSetup(MeshComp, Body, *Setup,
			BoneWorld.Location, BoneWorld.Rotation,
			ECollisionChannel::SkeletalMesh, ECollisionEnabled::QueryOnly,
			ComponentWorldScaleAtStart, false, true);

		if (!bCreated)
		{
			Bodies.pop_back();
			continue;
		}

		Body.SetKinematic(true);
		Body.SetGravityEnabled(false);

		BodyToBoneIndex.push_back(MeshComp->FindBoneIndex(BoneName));
	}

	bInitialized = true;
}

void FHitboxInstance::Release(FPhysicsScene* Scene)
{
	if (Scene)
	{
		for (FBodyInstance& Body : Bodies)
		{
			Scene->DestroyBody(Body);
		}
	}
	Bodies.clear();
	BodyToBoneIndex.clear();
	ComponentWorldScaleAtStart = FVector::OneVector;
	bInitialized = false;
}

void FHitboxInstance::SyncBodiesFromBones(USkeletalMeshComponent* MeshComp)
{
	if (!bInitialized || !MeshComp) return;

	const int32 Count = static_cast<int32>(Bodies.size());
	for (int32 i = 0; i < Count; ++i)
	{
		FBodyInstance& Body = Bodies[i];
		if (!Body.IsValidBody()) continue;

		const int32 BoneIndex = BodyToBoneIndex[i];
		if (BoneIndex < 0) continue;

		FTransform BoneWorld;
		if (!MeshComp->GetBoneWorldTransformByIndex(BoneIndex, BoneWorld)) continue;

		Body.SetBodyTransform(BoneWorld);
	}
}
