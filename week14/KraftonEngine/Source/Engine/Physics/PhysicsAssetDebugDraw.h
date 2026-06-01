#pragma once

#include "Core/Types/EngineTypes.h"
#include "Physics/BodySetup.h"

class UWorld;
class UPhysicsAsset;
class USkinnedMeshComponent;

struct FPhysicsAssetDebugDrawOptions
{
	bool bDrawBodies = true;
	bool bDrawConstraints = true;
	FColor BodyColor = FColor::Yellow();
	FColor ConstraintColor = FColor::Green();
	FColor HighlightColor = FColor(255, 160, 0);

	int32 HighlightBodyIndex = -1;
	EPhysicsAssetShapeType HighlightShapeType = EPhysicsAssetShapeType::Sphere;
	int32 HighlightShapeIndex = -1;
	int32 HighlightConstraintIndex = -1;
};

void DrawPhysicsAssetDebug(UWorld* World, const UPhysicsAsset* Asset, const USkinnedMeshComponent* MeshComp, const FColor& Color = FColor::Yellow());
void DrawPhysicsAssetDebug(UWorld* World, const UPhysicsAsset* Asset, const USkinnedMeshComponent* MeshComp, const FPhysicsAssetDebugDrawOptions& Options);
