#pragma once

#include "Object/FName.h"
#include "Physics/PhysicsAssetEditTypes.h"
#include "Physics/PhysicsConstraintTemplate.h"

class UBodySetup;
class UPhysicsAsset;
struct FSkeletalMesh;

class FPhysicsAssetEditingLibrary
{
public:
	static UBodySetup* AddPrimitiveToBone(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, FName BoneName,
		const FPhysicsAssetBodyShapeDesc& ShapeDesc, EAngularConstraintMode ConstraintMode = EAngularConstraintMode::Limited);

	static bool RemoveBodyAndReconnectConstraints(UPhysicsAsset& Asset, const FSkeletalMesh& Mesh, FName BoneName);
};
