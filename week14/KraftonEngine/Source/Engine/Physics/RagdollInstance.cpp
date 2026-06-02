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

namespace
{
	FVector ScaleVectorComponentWise(const FVector& Value, const FVector& Scale)
	{
		return FVector(Value.X * Scale.X, Value.Y * Scale.Y, Value.Z * Scale.Z);
	}

	FTransform ScaleConstraintFrame(const FTransform& Frame, const FVector& Scale)
	{
		FTransform Result = Frame;
		Result.Location = ScaleVectorComponentWise(Frame.Location, Scale);
		return Result;
	}
}

void FRagdollInstance::Initialize(UPhysicsAsset* Asset, USkeletalMeshComponent* MeshComp, FPhysicsScene* Scene)
{
	if (bInitialized || !Asset || !MeshComp || !Scene)
	{
		return;
	}

	USkeletalMesh* Mesh = MeshComp->GetSkeletalMesh();
	FSkeletalMesh* SkeletalAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!SkeletalAsset)
	{
		return;
	}

	const int32 NumBones = static_cast<int32>(SkeletalAsset->Bones.size());
	InitialLocalPose.clear();
	InitialLocalPose.reserve(NumBones);
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		InitialLocalPose.push_back(MeshComp->GetBoneLocalTransformByIndex(BoneIndex));
	}
	ComponentWorldScaleAtStart = MeshComp->GetWorldScale();

	const TArray<UBodySetup*>& BodySetups = Asset->GetBodySetups();
	const int32 NumSetups = static_cast<int32>(BodySetups.size());

	Bodies.reserve(NumSetups);
	BodyToBoneIndex.reserve(NumSetups);

	TMap<FString, int32> BoneToBody;

	for (UBodySetup* Setup : BodySetups)
	{
		if (!Setup) continue;
		if (!Asset->IsBodyCollisionEnabled(Setup->GetBoneName()))
		{
			continue;
		}

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
			ComponentWorldScaleAtStart, false, true);

		if (!bCreated)
		{
			Bodies.pop_back();
			continue;
		}

		const int32 BodyIndex = static_cast<int32>(Bodies.size()) - 1;
		BodyToBoneIndex.push_back(MeshComp->FindBoneIndex(BoneName));
		BoneToBody[BoneName] = BodyIndex;
	}

	for (UPhysicsConstraintTemplate* Constraint : Asset->GetConstraintTemplates())
	{
		if (!Constraint) continue;

		auto ItParent = BoneToBody.find(Constraint->GetParentBoneName().ToString());
		auto ItChild  = BoneToBody.find(Constraint->GetChildBoneName().ToString());
		if (ItParent == BoneToBody.end() || ItChild == BoneToBody.end())
		{
			continue;
		}

		const FTransform LocalFrameA = ScaleConstraintFrame(Constraint->GetLocalFrameA(), ComponentWorldScaleAtStart);
		const FTransform LocalFrameB = ScaleConstraintFrame(Constraint->GetLocalFrameB(), ComponentWorldScaleAtStart);
		const bool bDisableCollision = Asset->IsCollisionDisabled(Constraint->GetParentBoneName(), Constraint->GetChildBoneName());

		FConstraintInstance* Inst = Scene->CreateD6Constraint(
			&Bodies[ItParent->second], &Bodies[ItChild->second],
			LocalFrameA, LocalFrameB,
			Constraint->GetAngularMode(),
			Constraint->GetSwing1Limit(), Constraint->GetSwing2Limit(), Constraint->GetTwistLimit(),
			bDisableCollision);

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
	InitialLocalPose.clear();
	ComponentWorldScaleAtStart = FVector::OneVector;
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
	if (NumBones == 0 || static_cast<int32>(InitialLocalPose.size()) != NumBones)
	{
		return;
	}

	const FMatrix CompWorldInv = MeshComp->GetWorldMatrix().GetInverse();
	const FTransform CompWorldNoScale(
		MeshComp->GetWorldLocation(),
		MeshComp->GetWorldRotation().ToQuaternion(),
		FVector::OneVector);
	const FMatrix CompWorldNoScaleInv = CompWorldNoScale.ToMatrix().GetInverse();

	TArray<FMatrix> CompGlobal;
	CompGlobal.resize(NumBones, FMatrix::Identity);
	TArray<bool> bSolved;
	bSolved.resize(NumBones, false);

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
		FMatrix BodyCompGlobal = BodyWorld.ToMatrix() * CompWorldNoScaleInv;
		BodyCompGlobal.SetLocation(CompWorldInv.TransformPositionWithW(BodyWorld.Location));
		CompGlobal[BoneIndex] = BodyCompGlobal;
		bSolved[BoneIndex]    = true;
	}

	for (int32 b = 0; b < NumBones; ++b)
	{
		if (bSolved[b])
		{
			continue;
		}

		const int32 Parent = Asset->Bones[b].ParentIndex;
		if (Parent >= 0 && Parent < b && bSolved[Parent])
		{
			CompGlobal[b] = InitialLocalPose[b].ToMatrix() * CompGlobal[Parent];
			bSolved[b]   = true;
		}
	}

	// Some FBX files put weighted vertices on a helper bone above the first simulated body
	// (for example Bip001 -> Pelvis). Infer those ancestors from the solved child body so
	// helper-weighted triangles do not stay at the bind pose and stretch into long spikes.
	for (int32 b = NumBones - 1; b >= 0; --b)
	{
		if (bSolved[b])
		{
			continue;
		}

		for (int32 Child = b + 1; Child < NumBones; ++Child)
		{
			if (Asset->Bones[Child].ParentIndex == b && bSolved[Child])
			{
				CompGlobal[b] = InitialLocalPose[Child].ToMatrix().GetInverse() * CompGlobal[Child];
				bSolved[b]   = true;
				break;
			}
		}
	}

	for (int32 b = 0; b < NumBones; ++b)
	{
		if (bSolved[b])
		{
			continue;
		}

		const int32 Parent = Asset->Bones[b].ParentIndex;
		CompGlobal[b] = (Parent >= 0 && Parent < b)
			? InitialLocalPose[b].ToMatrix() * CompGlobal[Parent]
			: InitialLocalPose[b].ToMatrix();
		bSolved[b] = true;
	}

	TArray<FTransform> LocalPose;
	LocalPose.resize(NumBones);

	for (int32 b = 0; b < NumBones; ++b)
	{
		const int32 Parent = Asset->Bones[b].ParentIndex;
		const FMatrix Local = (Parent >= 0)
			? CompGlobal[b] * CompGlobal[Parent].GetInverse()
			: CompGlobal[b];

		LocalPose[b] = FTransform(Local);
		LocalPose[b].Scale = InitialLocalPose[b].Scale;
	}

	MeshComp->SetBoneLocalTransforms(LocalPose);
}
