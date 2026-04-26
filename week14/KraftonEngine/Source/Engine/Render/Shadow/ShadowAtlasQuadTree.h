#pragma once
#include "Math/Vector.h"
#include "Core/ClassTypes.h"

// Exclusive to spotlights
struct FLightInfo;

struct Node {
	// Dimensions
	FVector2		TopLeft;
	float			Resolution;

	// Data
	bool			bOccupied	= false;				// True if and only if this node is allocated to a shadow map by whole
	bool			bSplit		= false;				// True if and only if this node has been split into a subtree
	int32			Children[4] = { -1, -1, -1, -1 };	// indices into Nodes[], -1 = no child
};

struct FAtlasRegion { uint32 X, Y, Size; bool bValid; };

// Buddy allocation quadtree for shadow atlas management
class FShadowAtlasQuadTree {
public:
	// Initializes the head node of the atlas, and define the minimum resolution for the shadow maps to be allocated.
	void Init(float InAtlasSize, float inMinShadowMapResolution);

	// Try allocating the uv region for the input light source
	FAtlasRegion Add(const FLightInfo& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H);

	// For non-camera dependent addition
	FAtlasRegion Add(const FLightInfo& InLightInfo);

	// Called every frame to reset the atlas.
	void Reset();

	// Hard clear including the root node
	void Clear();

	// Accessor
	float GetAtlasSize() const { return AtlasSize; }

private:
	// Allocates the node at NodeIdx and returns the corresponding atlas region. Returns invalid region if the node is occupied or too small.
	FAtlasRegion AllocateNode(int32 NodeIdx, uint32 RequestedSize);

	// Greedily splits the quadtree to find the best fit for the new shadow map
	bool  Split(int32 Idx);

	// Ranks the importance of the input light source based on its properties.
	float EvaluateResolution(const FLightInfo& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H) const;

	// Non-camera dependent version
	float EvaluateResolution(const FLightInfo& InLightInfo);

private:
	TArray<Node> Nodes;
	float		 AtlasSize				= 4096.f;
	float		 MinShadowMapResolution = 64.f;
};