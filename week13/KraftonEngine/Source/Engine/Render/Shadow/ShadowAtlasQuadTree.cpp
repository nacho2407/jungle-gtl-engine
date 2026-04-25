#include "ShadowAtlasQuadTree.h"
#include "Render/Types/ForwardLightData.h"

// Public functions
void FShadowAtlasQuadTree::Init(float InAtlasSize, float InMinShadowMapResolution) {
	if (InMinShadowMapResolution <= 0.f || InAtlasSize <= 0.f) {
		return;
	}
	if (InMinShadowMapResolution > InAtlasSize) {
		return;
	}

	AtlasSize = InAtlasSize;
	MinShadowMapResolution = InMinShadowMapResolution;
	Node RootNode = {};
	RootNode.TopLeft = FVector2(0.f, 0.f);
	RootNode.Resolution = InAtlasSize;

	Nodes.push_back(RootNode);
}

FAtlasRegion FShadowAtlasQuadTree::Add(const FLightInfo& InLightInfo) {
	if (Nodes.empty()) return { 0, 0, 0, false };

	uint32 RequestedSize = static_cast<uint32>(EvaluateResolution(InLightInfo));
	return AllocateNode(0, RequestedSize);
}

void FShadowAtlasQuadTree::Reset() {
	if (!Nodes.empty()) {
		Nodes.resize(1);
		Node& Root = Nodes[0];
		Root.bOccupied = false;
		Root.bSplit = false;
		Root.Children[0] = Root.Children[1] = Root.Children[2] = Root.Children[3] = -1;
	}
}

void FShadowAtlasQuadTree::Clear() {
	Nodes.clear();
}

// Private helpers
FAtlasRegion FShadowAtlasQuadTree::AllocateNode(int32 NodeIdx, uint32 RequestedSize) {
	if (NodeIdx < 0
		|| NodeIdx >= Nodes.size()
		|| Nodes[NodeIdx].bOccupied) {
		// Invalid Node index
		return { 0, 0, 0, false };
	}

	Node node = Nodes[NodeIdx];
	if (node.bSplit) {
		for (int32 SubIdx : node.Children) {
			// Greedily allocate the first child node that can fit the requested size
			FAtlasRegion AllocatedRegion = AllocateNode(SubIdx, RequestedSize);
			if (AllocatedRegion.bValid) {
				return AllocatedRegion;
			}
		}

		return { 0, 0, 0, false };
	}
	else {
		if (node.Resolution == RequestedSize) {
			Nodes[NodeIdx].bOccupied = true;
			return { static_cast<uint32> (node.TopLeft.X), static_cast<uint32> (node.TopLeft.Y), static_cast<uint32>(node.Resolution), true };
		}
		else {

			// Try again after splitting
			if (Split(NodeIdx)) {
				return AllocateNode(NodeIdx, RequestedSize);
			}
		}
	}

	return { 0, 0, 0, false };
}

bool FShadowAtlasQuadTree::Split(int32 Idx) {
	if (Idx < 0 || Idx >= (int32)Nodes.size()) return false;

	float HalfRes = Nodes[Idx].Resolution / 2.f;
	if (HalfRes < MinShadowMapResolution) {
		// Reached the minimum resolution. Should not split further.
		return false;
	}

	FVector2 TL = Nodes[Idx].TopLeft;
	for (int i = 0; i < 4; i++) {
		Node NewNode = {};
		NewNode.Resolution = HalfRes;

		switch (i) {
		case (0): // Top-Left
			NewNode.TopLeft = TL;
			break;
		case (1): // Top-Right
			NewNode.TopLeft = TL + FVector2(HalfRes, 0);
			break;
		case (2): // Bottom-Left
			NewNode.TopLeft = TL + FVector2(0, HalfRes);
			break;
		case (3): // Bottom-Right
			NewNode.TopLeft = TL + FVector2(HalfRes, HalfRes);
			break;
		}
		Nodes.push_back(NewNode);
		Nodes[Idx].Children[i] = Nodes.size() - 1;
	}
	Nodes[Idx].bSplit = true;
	return true;
}

float FShadowAtlasQuadTree::EvaluateResolution(const FLightInfo& InLightInfo) const {
	FVector4 Color = InLightInfo.Color;
	float    Intensity = InLightInfo.Intensity;
	float    Radius = InLightInfo.AttenuationRadius;
	float    Falloff = InLightInfo.FalloffExponent;

	// Spotlight evaluation
	float Score = 0.f;
	Score += Color.X * 0.2126f + Color.Y * 0.7152f + Color.Z * 0.0722f; // Luminance
	Score *= Intensity;
	Score *= Radius;
	Score += pow(Falloff, 2.f);

	uint32 res = 64;
	if (Score > 50.f)  res = 128;
	if (Score > 200.f) res = 256;
	if (Score > 800.f) res = 512;
	return res;
}