#pragma once

#include "CoreMinimal.h"
#include "EngineAPI.h"
#include "Core/ShowFlags.h"

class UScene;
class FFrustum;
struct FRenderCommandQueue;
class UPrimitiveComponent;

class ENGINE_API FSceneRenderCollector
{
public:
	void CollectRenderCommands(UScene* Scene, const FFrustum& Frustum,
		const FShowFlags& ShowFlags, const FVector& CameraPosition, FRenderCommandQueue& OutQueue);

private:
	void FrustrumCull(UScene* Scene, const FFrustum& Frustum,
		const FShowFlags& ShowFlags, TArray<UPrimitiveComponent*>& OutVisible);
};
