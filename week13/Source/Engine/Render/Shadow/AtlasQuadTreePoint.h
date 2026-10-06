#pragma once
#include "AtlasQuadTreeBase.h"

class FAtlasQuadTreePoint : public FAtlasQuadTreeBase {
public:
	void AddToBatch(const FPointLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H, int32 LightIdx = -1);
	void AddToBatch(const FPointLightParams& InLightInfo, float OverrideResolution, int32 LightIdx = -1);

	// Sorts the pending batch by evaluated resolution, then allocates all entries into the atlas.
	TArray<FAtlasRegion> CommitBatch() override;

	// Returns the snapped power-of-two resolution for a light (same formula CommitBatch uses internally).
	uint32 ComputeSnappedResolution(const FPointLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H) const;

private:
	// Ranks the importance of the input light source based on its properties.
	float EvaluateResolution(const FPointLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H) const;

private:
	struct FBatchEntry { FPointLightParams Light; float Resolution; int32 LightIdx; };
	TArray<FBatchEntry> Batch;
};