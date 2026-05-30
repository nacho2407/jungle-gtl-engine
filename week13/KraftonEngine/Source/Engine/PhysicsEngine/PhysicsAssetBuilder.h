#pragma once

#include "PhysicsAsset.h"

class USkeletalMesh;

struct FPhysicsAssetBuildOptions
{
	bool bSkipRootBody = false;

	float DefaultBodyRadius = 5.0f;
	float DefaultBodyLength = 20.0f;
	float DefaultBoxSize = 10.0f;
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
		const FPhysicsAssetBuildOptions& Options);
};
