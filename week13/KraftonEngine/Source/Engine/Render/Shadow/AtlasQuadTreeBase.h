#pragma once
#include "Math/Vector.h"
#include "Core/ClassTypes.h"
#include "Render/Types/GlobalLightParams.h"

struct Node {
	// Dimensions
	FVector2		TopLeft;
	float			Resolution;

	// Data
	bool			bOccupied   = false;				// True if and only if this node is allocated to a shadow map by whole
	bool			bSplit	    = false;				// True if and only if this node has been split into a subtree
	int32			Children[4] = { -1, -1, -1, -1 };	// indices into Nodes[], -1 = no child
};

// LightIdx tracks which point this AtlasRegion belongs to. -1 mean spotlight.
// FaceIdx tracks which face of the point cubemap this AtlasRegion belongs to.
struct FAtlasRegion { uint32 X, Y, Size; bool bValid; int32 LightIdx = -1; ECubeMapOrientation FaceIdx = ECubeMapOrientation::CMO_Unknown; uint32 PageIdx = 0; };

class FAtlasQuadTreeBase {
	using enum ECubeMapOrientation;
public:
	virtual ~FAtlasQuadTreeBase() = default;

	// Initializes the head node of the atlas, and define the minimum resolution for the shadow maps to be allocated.
	void Init(float InAtlasSize, float inMinShadowMapResolution);

	// Sorts the pending batch by evaluated resolution, then allocates all entries into the atlas.
	virtual TArray<FAtlasRegion> CommitBatch() = 0;

	// Called every frame to reset the atlas.
	void Reset();

	// Hard clear including the root node
	void Clear();

	// Accessor
	float GetAtlasSize() const { return AtlasSize; }
	float GetMinResolution() const { return MinShadowMapResolution; }

	// Math helpers (public — 해상도 스케일링에서 외부 사용)
	uint32 NextPowerOfTwo(uint32 v) const;
	uint32 RoundToNearestPowerOfTwo(uint32 Value) const;

protected:
	// Allocates the node at NodeIdx and returns the corresponding atlas region. Returns invalid region if the node is occupied or too small.
	// OwnerIdx = -1 means spotlight
	FAtlasRegion AllocateNode(int32 NodeIdx, uint32 RequestedSize, int32 OwnerIdx = -1, ECubeMapOrientation FaceIdx = CMO_Unknown);

	// Greedily splits the quadtree to find the best fit for the new shadow map
	bool  Split(int32 Idx);

protected:
	TArray<Node> Nodes;
	float		 AtlasSize = 4096.f;
	float		 MinShadowMapResolution = 64.f;

	float		 RemainingSpace = 4096.f * 4096.f;

	const float  z_guard = 5.f;
};