#pragma once

#include "Core/Types/EngineTypes.h"

class UWorld;
class UPhysicsAsset;
class USkinnedMeshComponent;

void DrawPhysicsAssetDebug(UWorld* World, const UPhysicsAsset* Asset, const USkinnedMeshComponent* MeshComp, const FColor& Color = FColor::Yellow());
