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
#include "Core/Logging/Log.h"

#include <cmath>

namespace
{
	constexpr int32 MaxRagdollDebugRows = 10;

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

	uint16 AllocateSelfCollisionGroup()
	{
		static uint16 NextGroup = 1;

		const uint16 Group = NextGroup++;
		if (NextGroup == 0)
		{
			NextGroup = 1;
		}
		return Group;
	}

	bool IsFiniteVector(const FVector& Value)
	{
		return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
	}

	bool IsFiniteQuat(const FQuat& Value)
	{
		return std::isfinite(Value.X) && std::isfinite(Value.Y) &&
			std::isfinite(Value.Z) && std::isfinite(Value.W);
	}

	bool IsFiniteTransform(const FTransform& Value)
	{
		return IsFiniteVector(Value.Location) && IsFiniteQuat(Value.Rotation) && IsFiniteVector(Value.Scale);
	}

	const char* BoolText(bool bValue)
	{
		return bValue ? "true" : "false";
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
	const uint16 SelfCollisionGroup = AllocateSelfCollisionGroup();
	int32 BodyLogCount = 0;

	UE_LOG("[RagdollDbg][0] init mesh=%s bones=%d bodySetups=%d constraints=%llu worldScale=(%.3f,%.3f,%.3f) selfCollisionGroup=%u",
		Mesh->GetAssetPathFileName().c_str(),
		NumBones,
		NumSetups,
		static_cast<unsigned long long>(Asset->GetConstraintTemplates().size()),
		ComponentWorldScaleAtStart.X, ComponentWorldScaleAtStart.Y, ComponentWorldScaleAtStart.Z,
		static_cast<unsigned int>(SelfCollisionGroup));

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
			ComponentWorldScaleAtStart, false, true, SelfCollisionGroup);

		if (!bCreated)
		{
			Bodies.pop_back();
			continue;
		}

		Body.SetMass(1.0f);
		Body.SetLinearDamping(0.05f);
		Body.SetAngularDamping(0.8f);

		const int32 BodyIndex = static_cast<int32>(Bodies.size()) - 1;
		const int32 BoneIndex = MeshComp->FindBoneIndex(BoneName);
		BodyToBoneIndex.push_back(BoneIndex);
		BoneToBody[BoneName] = BodyIndex;

		if (BodyLogCount < MaxRagdollDebugRows)
		{
			const FTransform CreatedWorld = Body.GetBodyTransform();
			const FVector Delta = CreatedWorld.Location - BoneWorld.Location;
			const FKAggregateGeom& Geom = Setup->GetAggGeom();

			UE_LOG("[RagdollDbg][2] body[%d] bone=%s boneIndex=%d shapes(box=%llu sphere=%llu capsule=%llu) boneWorld=(%.3f,%.3f,%.3f) bodyWorld=(%.3f,%.3f,%.3f) deltaLen=%.5f mass=%.3f finite=%s",
				BodyIndex,
				BoneName.c_str(),
				BoneIndex,
				static_cast<unsigned long long>(Geom.BoxElems.size()),
				static_cast<unsigned long long>(Geom.SphereElems.size()),
				static_cast<unsigned long long>(Geom.SphylElems.size()),
				BoneWorld.Location.X, BoneWorld.Location.Y, BoneWorld.Location.Z,
				CreatedWorld.Location.X, CreatedWorld.Location.Y, CreatedWorld.Location.Z,
				Delta.Length(),
				Body.GetMass(),
				BoolText(IsFiniteTransform(CreatedWorld)));
			++BodyLogCount;
		}
	}

	int32 ConstraintLogCount = 0;
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

		FConstraintInstance* Inst = Scene->CreateD6Constraint(
			&Bodies[ItParent->second], &Bodies[ItChild->second],
			LocalFrameA, LocalFrameB,
			Constraint->GetAngularMode(),
			Constraint->GetSwing1Limit(), Constraint->GetSwing2Limit(), Constraint->GetTwistLimit());

		if (Inst)
		{
			Constraints.push_back(Inst);
		}

		if (ConstraintLogCount < MaxRagdollDebugRows)
		{
			UE_LOG("[RagdollDbg][3] constraint[%d] parent=%s child=%s created=%s frameA.loc=(%.3f,%.3f,%.3f) frameB.loc=(%.3f,%.3f,%.3f) swing=(%.1f,%.1f) twist=%.1f",
				ConstraintLogCount,
				Constraint->GetParentBoneName().ToString().c_str(),
				Constraint->GetChildBoneName().ToString().c_str(),
				BoolText(Inst != nullptr),
				LocalFrameA.Location.X, LocalFrameA.Location.Y, LocalFrameA.Location.Z,
				LocalFrameB.Location.X, LocalFrameB.Location.Y, LocalFrameB.Location.Z,
				Constraint->GetSwing1Limit(), Constraint->GetSwing2Limit(), Constraint->GetTwistLimit());
			++ConstraintLogCount;
		}
	}

	bInitialized = true;
	DebugSyncFramesRemaining = 3;
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
	DebugSyncFramesRemaining = 0;
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
	const bool bLogSync = DebugSyncFramesRemaining > 0;
	const int32 DebugFrameIndex = bLogSync ? (4 - DebugSyncFramesRemaining) : 0;
	int32 BodySyncLogCount = 0;

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

		if (bLogSync && BodySyncLogCount < MaxRagdollDebugRows)
		{
			const FVector CompLoc = CompGlobal[BoneIndex].GetLocation();
			const FVector CompScale = CompGlobal[BoneIndex].GetScale();
			UE_LOG("[RagdollDbg][4] syncBody frame=%d body=%d bone=%s world=(%.3f,%.3f,%.3f) compGlobal=(%.3f,%.3f,%.3f) compScale=(%.3f,%.3f,%.3f) finite=%s",
				DebugFrameIndex,
				i,
				Asset->Bones[BoneIndex].Name.c_str(),
				BodyWorld.Location.X, BodyWorld.Location.Y, BodyWorld.Location.Z,
				CompLoc.X, CompLoc.Y, CompLoc.Z,
				CompScale.X, CompScale.Y, CompScale.Z,
				BoolText(IsFiniteTransform(BodyWorld)));
			++BodySyncLogCount;
		}
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

	if (bLogSync)
	{
		int32 LocalSyncLogCount = 0;
		for (int32 i = 0; i < static_cast<int32>(Bodies.size()) && LocalSyncLogCount < MaxRagdollDebugRows; ++i)
		{
			const int32 BoneIndex = BodyToBoneIndex[i];
			if (BoneIndex < 0 || BoneIndex >= NumBones)
			{
				continue;
			}

			const FTransform& Local = LocalPose[BoneIndex];
			const FTransform& Initial = InitialLocalPose[BoneIndex];
			const FVector Delta = Local.Location - Initial.Location;

			UE_LOG("[RagdollDbg][5] syncLocal frame=%d bone=%s localLoc=(%.3f,%.3f,%.3f) initLoc=(%.3f,%.3f,%.3f) deltaLen=%.5f localScale=(%.3f,%.3f,%.3f) finite=%s",
				DebugFrameIndex,
				Asset->Bones[BoneIndex].Name.c_str(),
				Local.Location.X, Local.Location.Y, Local.Location.Z,
				Initial.Location.X, Initial.Location.Y, Initial.Location.Z,
				Delta.Length(),
				Local.Scale.X, Local.Scale.Y, Local.Scale.Z,
				BoolText(IsFiniteTransform(Local)));
			++LocalSyncLogCount;
		}

		--DebugSyncFramesRemaining;
	}

	MeshComp->SetBoneLocalTransforms(LocalPose);
}
