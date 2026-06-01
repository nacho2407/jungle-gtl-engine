#pragma once

#include "Core/Types/EngineTypes.h"

class UWorld;
class UPhysicsAsset;
class USkinnedMeshComponent;

struct FPhysicsAssetDebugDrawOptions
{
	bool bDrawBodies = true;
	bool bDrawConstraints = true;
	FColor BodyColor = FColor::Yellow();
	FColor ConstraintColor = FColor::Green();
};

void DrawPhysicsAssetDebug(UWorld* World, const UPhysicsAsset* Asset, const USkinnedMeshComponent* MeshComp, const FColor& Color = FColor::Yellow());
void DrawPhysicsAssetDebug(UWorld* World, const UPhysicsAsset* Asset, const USkinnedMeshComponent* MeshComp, const FPhysicsAssetDebugDrawOptions& Options);
