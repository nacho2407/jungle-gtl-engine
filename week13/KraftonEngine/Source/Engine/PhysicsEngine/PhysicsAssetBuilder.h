#pragma once

#include "Engine/Math/Vector.h"
#include "PhysicsAsset.h"

class USkeletalMesh;

struct FPhysicsAssetBuildOptions
{
	bool bSkipRootBody = false;

	float DefaultBodyRadius = 5.0f;
	float DefaultBodyLength = 20.0f;
	float DefaultBoxSize = 10.0f;
	float MinBoneSize = 20.0f;
	float MinPrimitiveSize = 0.1f;
	float FitPadding = 1.01f;
	bool bUseDominantBoneWeight = true;
};

class FPhysicsAssetBuilder
{
public:
	static UPhysicsAsset* CreateFromSkeletalMesh(
		USkeletalMesh* SkeletalMesh,
		const FPhysicsAssetBuildOptions& Options = FPhysicsAssetBuildOptions());

private:
	static void CreateBodies(
		UPhysicsAsset* PhysicsAsset,
		USkeletalMesh* SkeletalMesh,
		const FPhysicsAssetBuildOptions& Options);

	static void AddDefaultShapeForBone(
		UBodySetup* BodySetup,
		const FString& BoneName,
		const FVector& FitCenter,
		const FVector& FitExtent,
		bool bHasFit,
		const FPhysicsAssetBuildOptions& Options);
};
