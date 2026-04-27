#pragma once
#include "AtlasQuadTreeBase.h"

class FAtlasQuadTreePoint : public FAtlasQuadTreeBase {
public:
	void AddToBatch(const FPointLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H);

	// Sorts the pending batch by evaluated resolution, then allocates all entries into the atlas.
	TArray<FAtlasRegion> CommitBatch() override;


private:
	// Ranks the importance of the input light source based on its properties.
	float EvaluateResolution(const FPointLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H) const;

private:
	TArray<std::pair<FPointLightParams, float>> Batch;
};