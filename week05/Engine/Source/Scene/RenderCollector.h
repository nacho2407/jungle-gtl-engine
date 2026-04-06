#pragma once

#include "CoreMinimal.h"
#include "EngineAPI.h"
#include "Core/ShowFlags.h"

class UScene;
class FFrustum;
struct FRenderCommandQueue;
class UPrimitiveComponent;
class FPrimitiveSceneProxy;

struct FVisiblePrimitiveEntry
{
	UPrimitiveComponent* PrimitiveComponent = nullptr;
	FPrimitiveSceneProxy* SceneProxy = nullptr;
	bool bStaticMesh = false;
};

class ENGINE_API FSceneRenderCollector
{
public:
	void CollectRenderCommands(UScene* Scene, const FFrustum& Frustum,
		const FShowFlags& ShowFlags, const FVector& CameraPosition, const FMatrix& ProjectionMatrix, FRenderCommandQueue& OutQueue);

private:
	void FrustrumCull(UScene* Scene, const FFrustum& Frustum,
		const FShowFlags& ShowFlags, const FVector& CameraPosition, const FMatrix& ProjectionMatrix, TArray<FVisiblePrimitiveEntry>& OutVisible);

	TArray<FVisiblePrimitiveEntry> VisiblePrimitivesScratch;
	TArray<UPrimitiveComponent*> CandidatePrimitivesScratch;
};
