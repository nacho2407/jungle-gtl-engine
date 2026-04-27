#pragma once
#include "AtlasQuadTreeBase.h"

// Exclusive to spotlights
struct FSpotLightParams;


// Buddy allocation quadtree for shadow atlas management
class FShadowAtlasQuadTree : public FAtlasQuadTreeBase {
public:
	void AddToBatch(const FSpotLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H);

	// Sorts the pending batch by evaluated resolution, then allocates all entries into the atlas.
	TArray<FAtlasRegion> CommitBatch() override;

private:
	// Ranks the importance of the input light source based on its properties.
	float EvaluateResolution(const FSpotLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H) const;

private:
	TArray<std::pair<FSpotLightParams, float>> Batch;
};