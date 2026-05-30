#include "PhysicsAssetBuilder.h"

#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Object/Object.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"

#include <algorithm>
#include <cctype>

UPhysicsAsset* FPhysicsAssetBuilder::CreateFromSkeletalMesh(
	USkeletalMesh* SkeletalMesh,
	const FPhysicsAssetBuildOptions& Options)
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		return nullptr;
	}

	UPhysicsAsset* PhysicsAsset = UObjectManager::Get().CreateObject<UPhysicsAsset>(SkeletalMesh);
	if (!PhysicsAsset)
	{
		return nullptr;
	}

	CreateBodies(PhysicsAsset, SkeletalMesh, Options);

	SkeletalMesh->SetPhysicsAsset(PhysicsAsset);
	return PhysicsAsset;
}

void FPhysicsAssetBuilder::CreateBodies(
	UPhysicsAsset* PhysicsAsset,
	USkeletalMesh* SkeletalMesh,
	const FPhysicsAssetBuildOptions& Options)
{
	if (!PhysicsAsset || !SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
	TArray<UBodySetup*>& BodySetups = PhysicsAsset->GetBodySetupsMutable();
	BodySetups.clear();

	for (const FBone& Bone : MeshAsset->Bones)
	{
		if (Options.bSkipRootBody && Bone.ParentIndex == -1)
		{
			continue;
		}

		UBodySetup* BodySetup = UObjectManager::Get().CreateObject<UBodySetup>(PhysicsAsset);
		if (!BodySetup)
		{
			continue;
		}

		BodySetup->BoneName = FName(Bone.Name);
		AddDefaultShapeForBone(BodySetup, Bone.Name, Options);

		BodySetups.push_back(BodySetup);
	}
}

void FPhysicsAssetBuilder::AddDefaultShapeForBone(
	UBodySetup* BodySetup,
	const FString& BoneName,
	const FPhysicsAssetBuildOptions& Options)
{
	if (!BodySetup)
	{
		return;
	}

	FString LowerName = BoneName;
	std::transform(
		LowerName.begin(),
		LowerName.end(),
		LowerName.begin(),
		[](unsigned char C) { return static_cast<char>(std::tolower(C)); });

	if (LowerName.find("head") != FString::npos)
	{
		FKSphereElem Sphere;
		Sphere.Radius = Options.DefaultBodyRadius;
		BodySetup->GetAggGeom().SphereElems.push_back(Sphere);
		return;
	}

	if (LowerName.find("root") != FString::npos ||
		LowerName.find("pelvis") != FString::npos ||
		LowerName.find("hip") != FString::npos)
	{
		FKBoxElem Box;
		Box.X = Options.DefaultBoxSize;
		Box.Y = Options.DefaultBoxSize;
		Box.Z = Options.DefaultBoxSize;
		BodySetup->GetAggGeom().BoxElems.push_back(Box);
		return;
	}

	FKSphylElem Sphyl;
	Sphyl.Radius = Options.DefaultBodyRadius;
	Sphyl.Length = Options.DefaultBodyLength;
	BodySetup->GetAggGeom().SphylElems.push_back(Sphyl);
}
