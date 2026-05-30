#include "RagdollInstance.h"

#include "Physics/PhysicsAsset.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsConstraintTemplate.h"
#include "Physics/PhysicsScene.h"
#include "Physics/ConstraintInstance.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Types/CollisionTypes.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Math/Matrix.h"

void FRagdollInstance::Initialize(UPhysicsAsset* Asset, USkeletalMeshComponent* MeshComp, FPhysicsScene* Scene)
{
	if (bInitialized || !Asset || !MeshComp || !Scene)
	{
		return;
	}

	const TArray<UBodySetup*>& BodySetups = Asset->GetBodySetups();
	const int32 NumSetups = static_cast<int32>(BodySetups.size());

	Bodies.reserve(NumSetups);
	BodyToBoneIndex.reserve(NumSetups);

	TMap<FString, int32> BoneToBody;

	// 바디 생성
	for (UBodySetup* Setup : BodySetups)
	{
		if (!Setup) continue;

		const FString BoneName = Setup->GetBoneName().ToString();

		FTransform BoneWorld;
		if (!MeshComp->GetBoneWorldTransformByName(BoneName, BoneWorld))
		{
			continue;
		}

		Bodies.emplace_back();
		FBodyInstance& Body = Bodies.back();
		Body.bSyncOwnerFromPhysics = false;

		const bool bCreated = Scene->CreateBodyFromSetup(MeshComp, Body, *Setup,
			BoneWorld.Location, BoneWorld.Rotation,
			ECollisionChannel::Pawn, ECollisionEnabled::QueryAndPhysics,
			FVector::OneVector, false, true);

		if (!bCreated)
		{
			Bodies.pop_back();
			continue;
		}

		const int32 BodyIndex = static_cast<int32>(Bodies.size()) - 1;
		BodyToBoneIndex.push_back(MeshComp->FindBoneIndex(BoneName));
		BoneToBody[BoneName] = BodyIndex;
	}

	// 조인트 생성
	for (UPhysicsConstraintTemplate* Constraint : Asset->GetConstraintTemplates())
	{
		if (!Constraint) continue;

		auto ItParent = BoneToBody.find(Constraint->GetParentBoneName().ToString());
		auto ItChild  = BoneToBody.find(Constraint->GetChildBoneName().ToString());
		if (ItParent == BoneToBody.end() || ItChild == BoneToBody.end())
		{
			continue; // 양쪽 바디가 다 있어야 연결
		}

		FConstraintInstance* Inst = Scene->CreateD6Constraint(
			&Bodies[ItParent->second], &Bodies[ItChild->second],
			Constraint->GetLocalFrameA(), Constraint->GetLocalFrameB(),
			Constraint->GetAngularMode(),
			Constraint->GetSwing1Limit(), Constraint->GetSwing2Limit(), Constraint->GetTwistLimit());

		if (Inst)
		{
			Constraints.push_back(Inst);
		}
	}

	bInitialized = true;
}

void FRagdollInstance::Release(FPhysicsScene* Scene)
{
	if (Scene)
	{
		// 조인트 먼저, 그 다음 바디 (의존 역순)
		for (FConstraintInstance* Inst : Constraints)
		{
			Scene->DestroyConstraint(Inst);
		}
		for (FBodyInstance& Body : Bodies)
		{
			Scene->DestroyBody(Body);
		}
	}

	Constraints.clear();
	Bodies.clear();
	BodyToBoneIndex.clear();
	bInitialized = false;
}

void FRagdollInstance::SyncBonesFromBodies(USkeletalMeshComponent* MeshComp)
{
	if (!bInitialized || !MeshComp)
	{
		return;
	}

	USkeletalMesh* Mesh  = MeshComp->GetSkeletalMesh();
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset)
	{
		return;
	}

	const int32 NumBones = static_cast<int32>(Asset->Bones.size());
	if (NumBones == 0)
	{
		return;
	}

	// 월드 → 컴포넌트 공간 변환
	const FMatrix CompWorldInv = MeshComp->GetWorldMatrix().GetInverse();

	// 각 본의 컴포넌트-공간 글로벌 행렬을 구한다. (parent-first 순서 가정)
	TArray<FMatrix> CompGlobal;
	CompGlobal.resize(NumBones, FMatrix::Identity);
	TArray<bool> bHasBody;
	bHasBody.resize(NumBones, false);

	// 1) 바디가 있는 본 → 바디의 월드 트랜스폼을 컴포넌트 공간으로
	for (int32 i = 0; i < static_cast<int32>(Bodies.size()); ++i)
	{
		if (!Bodies[i].IsValidBody())
		{
			continue;
		}

		const int32 BoneIndex = BodyToBoneIndex[i];
		if (BoneIndex < 0 || BoneIndex >= NumBones)
		{
			continue;
		}

		const FTransform BodyWorld = Bodies[i].GetBodyTransform();
		CompGlobal[BoneIndex] = BodyWorld.ToMatrix() * CompWorldInv;
		bHasBody[BoneIndex]   = true;
	}

	// 2) 바디 없는 본 → 부모를 따라간다(레퍼런스 로컬 × 부모 글로벌)
	for (int32 b = 0; b < NumBones; ++b)
	{
		if (bHasBody[b])
		{
			continue;
		}

		const int32 Parent = Asset->Bones[b].ParentIndex;
		CompGlobal[b] = (Parent >= 0 && Parent < b)
			? Asset->Bones[b].GetReferenceLocalPose() * CompGlobal[Parent]
			: Asset->Bones[b].GetReferenceLocalPose(); // 루트
	}

	// 3) 컴포넌트 글로벌 → 로컬(부모 상대)로 변환 → 포즈 주입
	TArray<FTransform> LocalPose;
	LocalPose.resize(NumBones);

	for (int32 b = 0; b < NumBones; ++b)
	{
		const int32 Parent = Asset->Bones[b].ParentIndex;
		const FMatrix Local = (Parent >= 0)
			? CompGlobal[b] * CompGlobal[Parent].GetInverse()
			: CompGlobal[b];
		LocalPose[b] = FTransform(Local);
	}

	MeshComp->SetBoneLocalTransforms(LocalPose);
}
