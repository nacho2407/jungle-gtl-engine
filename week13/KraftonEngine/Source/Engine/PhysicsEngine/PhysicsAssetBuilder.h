#pragma once

#include "Core/Types/CoreTypes.h"
#include "Engine/Math/Vector.h"
#include "PhysicsAsset.h"

class USkeletalMesh;

enum class EPhysicsAssetFitGeomType : uint8
{
	Box,
	Sphyl,
	Sphere
};

struct FPhysicsAssetBuildOptions
{
	bool bSkipRootBody = false;

	EPhysicsAssetFitGeomType GeomType = EPhysicsAssetFitGeomType::Sphyl;
	float MinBoneSize = 20.0f;
	float MinPrimitiveSize = 0.1f;
	float FitPadding = 1.01f;
	bool bUseDominantBoneWeight = true;
	bool bAutoOrientToBone = true;
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

	static void AddFittedShapeForBone(
		UBodySetup* BodySetup,
		const FVector& FitCenter,
		const FVector& FitExtent,
		const TArray<FVector>& FitPositions,
		const FPhysicsAssetBuildOptions& Options);
};
