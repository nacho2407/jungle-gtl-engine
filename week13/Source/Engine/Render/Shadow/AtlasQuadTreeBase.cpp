#include "AtlasQuadTreeBase.h"

void FAtlasQuadTreeBase::Init(float InAtlasSize, float inMinShadowMapResolution) {
	if (inMinShadowMapResolution <= 0.f || InAtlasSize <= 0.f) {
		return;
	}
	if (inMinShadowMapResolution > InAtlasSize) {
		return;
	}

	AtlasSize = InAtlasSize;
	RemainingSpace = AtlasSize * AtlasSize;
	MinShadowMapResolution = inMinShadowMapResolution;
	Node RootNode = {};
	RootNode.TopLeft = FVector2(0.f, 0.f);
	RootNode.Resolution = InAtlasSize;

	Nodes.push_back(RootNode);
}

void FAtlasQuadTreeBase::Reset() {
	if (!Nodes.empty()) {
		Nodes.resize(1);
		Node& Root = Nodes[0];
		Root.bOccupied = false;
		Root.bSplit = false;
		Root.Children[0] = Root.Children[1] = Root.Children[2] = Root.Children[3] = -1;
	}

	RemainingSpace = AtlasSize * AtlasSize;
}

void FAtlasQuadTreeBase::Clear() {
	Nodes.clear();
	RemainingSpace = AtlasSize * AtlasSize;
}

FAtlasRegion FAtlasQuadTreeBase::AllocateNode(int32 NodeIdx, uint32 RequestedSize, int32 OwnerIdx, ECubeMapOrientation FaceIdx) {
	if (NodeIdx < 0
		|| NodeIdx >= Nodes.size()
		|| Nodes[NodeIdx].bOccupied
		|| RequestedSize < (uint32)(MinShadowMapResolution)
		|| RemainingSpace <= 0.f) {
		// Invalid Node index
		return { 0, 0, 0, false, OwnerIdx };
	}

	if (RequestedSize * RequestedSize >= RemainingSpace && RequestedSize > MinShadowMapResolution)
		return AllocateNode(NodeIdx, RequestedSize / 2, OwnerIdx, FaceIdx);

	Node node = Nodes[NodeIdx];
	if (node.bSplit) {
		for (int32 SubIdx : node.Children) {
			if (Nodes[SubIdx].bOccupied || Nodes[SubIdx].Resolution < RequestedSize) continue;

			// Greedily allocate the first child node that can fit the requested size
			FAtlasRegion AllocatedRegion = AllocateNode(SubIdx, RequestedSize, OwnerIdx, FaceIdx);
			if (AllocatedRegion.bValid) {
				return AllocatedRegion;
			}
		}

		return { 0, 0, 0, false, OwnerIdx };
	}
	else {
		if (node.Resolution == RequestedSize) {
			RemainingSpace -= RequestedSize * RequestedSize;
			Nodes[NodeIdx].bOccupied = true;
			return { static_cast<uint32> (node.TopLeft.X), static_cast<uint32> (node.TopLeft.Y), static_cast<uint32>(node.Resolution), true, OwnerIdx, FaceIdx };
		}
		else {

			// Try again after splitting
			if (Split(NodeIdx)) {
				return AllocateNode(NodeIdx, RequestedSize, OwnerIdx, FaceIdx);
			}
		}
	}

	return { 0, 0, 0, false, OwnerIdx };
}

bool FAtlasQuadTreeBase::Split(int32 Idx) {
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
		Nodes[Idx].Children[i] = (int32)(Nodes.size() - 1);
	}
	Nodes[Idx].bSplit = true;
	return true;
}

uint32 FAtlasQuadTreeBase::NextPowerOfTwo(uint32 v) const
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

uint32 FAtlasQuadTreeBase::RoundToNearestPowerOfTwo(uint32 Value) const
{
	if (Value <= 1)
		return 1;

	uint32 Upper = NextPowerOfTwo(Value);
	uint32 Lower = Upper >> 1;

	return (Value - Lower < Upper - Value) ? Lower : Upper;
}