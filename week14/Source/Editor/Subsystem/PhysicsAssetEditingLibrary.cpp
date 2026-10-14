#include "PhysicsAssetEditingLibrary.h"

#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Physics/BodySetup.h"
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsConstraintTemplate.h"

namespace
{
	struct FPendingConstraintReconnect
	{
		FName ChildBoneName = FName::None;
		int32 ChildBoneIndex = -1;
		EAngularConstraintMode AngularMode = EAngularConstraintMode::Limited;
		float Swing1Limit = 45.0f;
		float Swing2Limit = 45.0f;
		float TwistLimit = 30.0f;
	};

	int32 FindBoneIndexByName(const FSkeletalMesh& Mesh, FName BoneName)
	{
		for (int32 Index = 0; Index < static_cast<int32>(Mesh.Bones.size()); ++Index)
		{
			if (FName(Mesh.Bones[Index].Name) == BoneName)
			{
				return Index;
			}
		}

		return -1;
	}

	int32 FindNearestBodyAncestorIndex(const UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, int32 BoneIndex)
	{
		const int32 NumBones = static_cast<int32>(Mesh.Bones.size());
		if (BoneIndex < 0 || BoneIndex >= NumBones)
		{
			return -1;
		}

		int32 ParentIndex = Mesh.Bones[BoneIndex].ParentIndex;
		for (int32 Depth = 0; ParentIndex >= 0 && ParentIndex < NumBones && Depth < NumBones; ++Depth)
		{
			const FName ParentBoneName(Mesh.Bones[ParentIndex].Name);
			if (Asset.FindBodySetup(ParentBoneName))
			{
				return ParentIndex;
			}

			ParentIndex = Mesh.Bones[ParentIndex].ParentIndex;
		}

		return -1;
	}

	void CopyConstraintSettings(const UPhysicsConstraintTemplate* Constraint, FPendingConstraintReconnect& OutReconnect)
	{
		if (!Constraint)
		{
			return;
		}

		OutReconnect.AngularMode = Constraint->GetAngularMode();
		OutReconnect.Swing1Limit = Constraint->GetSwing1Limit();
		OutReconnect.Swing2Limit = Constraint->GetSwing2Limit();
		OutReconnect.TwistLimit = Constraint->GetTwistLimit();
	}

	const UPhysicsConstraintTemplate* FindConstraint(const UPhysicsAsset& Asset, FName ParentBone, FName ChildBone)
	{
		const int32 ConstraintIndex = Asset.FindConstraintIndex(ParentBone, ChildBone);
		const TArray<UPhysicsConstraintTemplate*>& Constraints = Asset.GetConstraintTemplates();
		if (ConstraintIndex < 0 || ConstraintIndex >= static_cast<int32>(Constraints.size()))
		{
			return nullptr;
		}

		return Constraints[ConstraintIndex];
	}

	void CreateReferencePoseConstraint(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, int32 ParentBoneIndex, const FPendingConstraintReconnect& Reconnect)
	{
		if (ParentBoneIndex < 0 || ParentBoneIndex >= static_cast<int32>(Mesh.Bones.size()))
		{
			return;
		}

		if (Reconnect.ChildBoneIndex < 0 || Reconnect.ChildBoneIndex >= static_cast<int32>(Mesh.Bones.size()))
		{
			return;
		}

		const FName ParentBoneName(Mesh.Bones[ParentBoneIndex].Name);
		if (Asset.FindConstraintIndex(ParentBoneName, Reconnect.ChildBoneName) >= 0)
		{
			return;
		}

		const FMatrix ChildGlobal = Mesh.Bones[Reconnect.ChildBoneIndex].GetReferenceGlobalPose();
		const FMatrix ParentGlobal = Mesh.Bones[ParentBoneIndex].GetReferenceGlobalPose();

		const FTransform FrameA(ChildGlobal * ParentGlobal.GetInverse());
		const FTransform FrameB;

		if (UPhysicsConstraintTemplate* NewConstraint = Asset.CreateConstraint(
			ParentBoneName,
			Reconnect.ChildBoneName,
			FrameA,
			FrameB,
			Reconnect.AngularMode))
		{
			NewConstraint->SetAngularMode(Reconnect.AngularMode);
			NewConstraint->SetAngularLimits(Reconnect.Swing1Limit, Reconnect.Swing2Limit, Reconnect.TwistLimit);
		}
	}

	bool IsShapeDescValid(const FPhysicsAssetBodyShapeDesc& ShapeDesc)
	{
		switch (ShapeDesc.PrimitiveType)
		{
		case EPhysicsAssetPrimitiveType::Sphere:
			return ShapeDesc.Extents.X > 0.0f;

		case EPhysicsAssetPrimitiveType::Box:
			return ShapeDesc.Extents.X > 0.0f
				&& ShapeDesc.Extents.Y > 0.0f
				&& ShapeDesc.Extents.Z > 0.0f;

		case EPhysicsAssetPrimitiveType::Capsule:
			return ShapeDesc.Extents.X > 0.0f
				&& ShapeDesc.Extents.Z > 0.0f;
		}

		return false;
	}

	bool AddShapeToBody(UBodySetup& Body, const FPhysicsAssetBodyShapeDesc& ShapeDesc)
	{
		if (!IsShapeDescValid(ShapeDesc))
		{
			return false;
		}

		switch (ShapeDesc.PrimitiveType)
		{
		case EPhysicsAssetPrimitiveType::Sphere:
			Body.AddSphere(ShapeDesc.Center, ShapeDesc.Extents.X);
			return true;

		case EPhysicsAssetPrimitiveType::Box:
			Body.AddBox(ShapeDesc.Center, ShapeDesc.Rotation, ShapeDesc.Extents);
			return true;

		case EPhysicsAssetPrimitiveType::Capsule:
			Body.AddSphyl(ShapeDesc.Center, ShapeDesc.Rotation, ShapeDesc.Extents.X, ShapeDesc.Extents.Z);
			return true;
		}

		return false;
	}
}

UBodySetup* FPhysicsAssetEditingLibrary::AddPrimitiveToBone(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, FName BoneName,
	const FPhysicsAssetBodyShapeDesc& ShapeDesc, EAngularConstraintMode ConstraintMode)
{
	const int32 BoneIndex = FindBoneIndexByName(Mesh, BoneName);
	if (BoneIndex < 0 || !IsShapeDescValid(ShapeDesc))
	{
		return nullptr;
	}

	UBodySetup* Body = Asset.GetOrCreateBodySetup(BoneName);
	if (!Body)
	{
		return nullptr;
	}

	if (!AddShapeToBody(*Body, ShapeDesc))
	{
		return nullptr;
	}

	const int32 ParentBoneIndex = FindNearestBodyAncestorIndex(Asset, Mesh, BoneIndex);
	if (ParentBoneIndex >= 0)
	{
		FPendingConstraintReconnect Reconnect;
		Reconnect.ChildBoneName = BoneName;
		Reconnect.ChildBoneIndex = BoneIndex;
		Reconnect.AngularMode = ConstraintMode;
		CreateReferencePoseConstraint(Asset, Mesh, ParentBoneIndex, Reconnect);
	}

	return Body;
}

bool FPhysicsAssetEditingLibrary::RemoveBodyAndReconnectConstraints(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, FName BoneName)
{
	const int32 RemovedBoneIndex = FindBoneIndexByName(Mesh, BoneName);
	if (RemovedBoneIndex < 0 || !Asset.FindBodySetup(BoneName))
	{
		return false;
	}

	TArray<FPendingConstraintReconnect> PendingReconnects;
	for (const UBodySetup* Body : Asset.GetBodySetups())
	{
		if (!Body || Body->GetBoneName() == BoneName)
		{
			continue;
		}

		const FName ChildBoneName = Body->GetBoneName();
		const int32 ChildBoneIndex = FindBoneIndexByName(Mesh, ChildBoneName);
		if (ChildBoneIndex < 0)
		{
			continue;
		}

		const int32 CurrentParentBodyIndex = FindNearestBodyAncestorIndex(Asset, Mesh, ChildBoneIndex);
		if (CurrentParentBodyIndex != RemovedBoneIndex)
		{
			continue;
		}

		FPendingConstraintReconnect Reconnect;
		Reconnect.ChildBoneName = ChildBoneName;
		Reconnect.ChildBoneIndex = ChildBoneIndex;
		CopyConstraintSettings(FindConstraint(Asset, BoneName, ChildBoneName), Reconnect);
		PendingReconnects.push_back(Reconnect);
	}

	if (!Asset.RemoveBodySetup(BoneName))
	{
		return false;
	}

	for (const FPendingConstraintReconnect& Reconnect : PendingReconnects)
	{
		const int32 NewParentBodyIndex = FindNearestBodyAncestorIndex(Asset, Mesh, Reconnect.ChildBoneIndex);
		CreateReferencePoseConstraint(Asset, Mesh, NewParentBodyIndex, Reconnect);
	}

	return true;
}
