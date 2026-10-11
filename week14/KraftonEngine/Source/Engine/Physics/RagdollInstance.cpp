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
#include "Profiling/Stats/Stats.h"

#include <string>

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
	
	bool ContainsText(const FString& Text, const char* Pattern)
	{
		return Text.find(Pattern) != std::string::npos;
	}

	int32 GetEntryAngularVelocityTargetScore(const FString& BoneName)
	{
		if (ContainsText(BoneName, "Pelvis") || ContainsText(BoneName, "pelvis")
			|| ContainsText(BoneName, "Hips") || ContainsText(BoneName, "hips"))
		{
			return 3;
		}

		if (ContainsText(BoneName, "Spine") || ContainsText(BoneName, "spine"))
		{
			return 2;
		}

		if (ContainsText(BoneName, "Root") || ContainsText(BoneName, "root"))
		{
			return 1;
		}

		return 0;
	}

	FVector ComputeEntryAngularVelocity(const FVector& InitialLinearVelocity)
	{
		FVector HorizontalVelocity(InitialLinearVelocity.X, InitialLinearVelocity.Y, 0.0f);

		FVector FallDirection = FVector::ForwardVector;
		if (HorizontalVelocity.Length() > 1.0f)
		{
			FallDirection = HorizontalVelocity.Normalized();
		}

		FVector FallAxis = FVector::UpVector.Cross(FallDirection);
		if (FallAxis.Length() <= 1.e-3f)
		{
			FallAxis = FVector::RightVector;
		}
		else
		{
			FallAxis.Normalize();
		}

		constexpr float EntryAngularSpeed = 4.0f;
		return FallAxis * EntryAngularSpeed;
	}
}

void FRagdollInstance::Initialize(UPhysicsAsset* Asset, USkeletalMeshComponent* MeshComp, FPhysicsScene* Scene, const FVector& InitialLinearVelocity)
{
	SCOPE_STAT_CAT("Ragdoll_Initialize", "Ragdoll");

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
	
	const FVector EntryAngularVelocity = ComputeEntryAngularVelocity(InitialLinearVelocity);
	int32 EntryAngularVelocityBodyIndex = -1;
	int32 EntryAngularVelocityBodyScore = -1;

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
		
		Body.SetLinearVelocity(InitialLinearVelocity);

		const int32 BodyIndex = static_cast<int32>(Bodies.size()) - 1;
		Body.SetGravityEnabled(Asset->IsBodyGravityEnabled(Setup->GetBoneName()));
		Body.SetMass(Asset->GetBodyMass(Setup->GetBoneName()));
		Body.SetLinearDamping(Asset->GetBodyLinearDamping(Setup->GetBoneName()));
		Body.SetAngularDamping(Asset->GetBodyAngularDamping(Setup->GetBoneName()));
		BodyToBoneIndex.push_back(MeshComp->FindBoneIndex(BoneName));
		BoneToBody[BoneName] = BodyIndex;
		
		const int32 BodyScore = GetEntryAngularVelocityTargetScore(BoneName);
		if (EntryAngularVelocityBodyIndex < 0 || BodyScore > EntryAngularVelocityBodyScore)
		{
			EntryAngularVelocityBodyIndex = BodyIndex;
			EntryAngularVelocityBodyScore = BodyScore;
		}
	}
	
	AnchorBodyIndex = EntryAngularVelocityBodyIndex;
	
	if (EntryAngularVelocityBodyIndex >= 0 && EntryAngularVelocityBodyIndex < static_cast<int32>(Bodies.size()))
	{
		Bodies[EntryAngularVelocityBodyIndex].SetAngularVelocity(EntryAngularVelocity);
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
	SCOPE_STAT_CAT("Ragdoll_Release", "Ragdoll");

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
	AnchorBodyIndex = -1;
	bInitialized = false;
}

bool FRagdollInstance::GetAnchorWorldLocation(FVector& OutWorldLocation) const
{
	if (!bInitialized)
	{
		return false;
	}

	if (AnchorBodyIndex < 0 || AnchorBodyIndex >= static_cast<int32>(Bodies.size()))
	{
		return false;
	}

	const FBodyInstance& AnchorBody = Bodies[AnchorBodyIndex];
	if (!AnchorBody.IsValidBody())
	{
		return false;
	}

	OutWorldLocation = AnchorBody.GetBodyTransform().Location;
	return true;
}

bool FRagdollInstance::BuildLocalPoseFromBodies(USkeletalMeshComponent* MeshComp, TArray<FTransform>& OutLocalPose) const
{
	SCOPE_STAT_CAT("Ragdoll_BuildLocalPose", "Ragdoll");

	OutLocalPose.clear();
	
	if (!bInitialized || !MeshComp)
	{
		return false;
	}

	USkeletalMesh* Mesh  = MeshComp->GetSkeletalMesh();
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!Asset)
	{
		return false;
	}

	const int32 NumBones = static_cast<int32>(Asset->Bones.size());
	if (NumBones == 0 || static_cast<int32>(InitialLocalPose.size()) != NumBones)
	{
		return false;
	}
	
	const FMatrix CompWorldInv = MeshComp->GetWorldMatrix().GetInverse();
	const FTransform CompWorldNoScale(MeshComp->GetWorldLocation(), MeshComp->GetWorldRotation().ToQuaternion(), FVector::OneVector);
	const FMatrix CompWorldNoScaleInv = CompWorldNoScale.ToMatrix().GetInverse();
	
	TArray<FMatrix> CompGlobal;
	CompGlobal.resize(NumBones, FMatrix::Identity);
	
	TArray<bool> bSolved;
	bSolved.resize(NumBones, false);
	
	// body의 world transform -> 메시 컴포넌트 기준 transform
	for (int32 i=0; i < static_cast<int32>(Bodies.size()); ++i)
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
		bSolved[BoneIndex] = true;
	}
	
	// 물리 바디 없어 아직 계산되지 않은 본을, 부모 본 transform을 이용해서 채움.
	for (int32 b=0; b < NumBones; ++b)
	{
		if (bSolved[b])
		{
			continue;
		}
		
		const int32 Parent = Asset->Bones[b].ParentIndex;
		if (Parent >= 0 && Parent < b && bSolved[Parent])
		{
			CompGlobal[b] = InitialLocalPose[b].ToMatrix() * CompGlobal[Parent];
			bSolved[b] = true;
		}
	}
	
	// 자식으로부터 부모를 거꾸로 추정
	for (int32 b=NumBones-1; b >= 0; --b)
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
				bSolved[b] = true;
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

	OutLocalPose.resize(NumBones);
	
	// CompGlobal 배열을 본 로컬 포즈 OutLocalPose로 변환
	for (int32 b=0; b < NumBones; ++b)
	{
		const int32 Parent = Asset->Bones[b].ParentIndex;
		const FMatrix Local = (Parent >= 0)
			? CompGlobal[b] * CompGlobal[Parent].GetInverse()
			: CompGlobal[b];

		OutLocalPose[b] = FTransform(Local);
		OutLocalPose[b].Scale = InitialLocalPose[b].Scale;
	}
	
	return true;
}

void FRagdollInstance::SyncBonesFromBodies(USkeletalMeshComponent* MeshComp)
{
	SCOPE_STAT_CAT("Ragdoll_SyncBones", "Ragdoll");

	TArray<FTransform> LocalPose;
	if (BuildLocalPoseFromBodies(MeshComp, LocalPose))
	{
		MeshComp->SetBoneLocalTransforms(LocalPose);
	}
}
