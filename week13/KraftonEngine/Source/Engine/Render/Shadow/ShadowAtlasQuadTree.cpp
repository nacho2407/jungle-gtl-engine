#include "ShadowAtlasQuadTree.h"
#include "Render/Types/ForwardLightData.h"

#include <algorithm>

namespace {
	uint32 NextPowerOfTwo(uint32 v)
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

	uint32 RoundToNearestPowerOfTwo(uint32 Value)
	{
		if (Value <= 1)
			return 1;

		uint32 Upper = NextPowerOfTwo(Value);
		uint32 Lower = Upper >> 1;

		return (Value - Lower < Upper - Value) ? Lower : Upper;
	}
}

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

FAtlasRegion FShadowAtlasQuadTree::Add(const FLightInfo& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H) {
	if (Nodes.empty()) return { 0, 0, 0, false };
	float RequestedSize = EvaluateResolution(InLightInfo, CameraPos, Forward, FOV, H);
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

float FShadowAtlasQuadTree::EvaluateResolution(const FLightInfo& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H) const {
	FVector4 Color		 = InLightInfo.Color;
	float   r_sphere;
	FVector c_sphere;

	// If outer angle is less than 60 degrees
	if (InLightInfo.OuterConeCos >= 0.5) {
		r_sphere = InLightInfo.AttenuationRadius / (2 * InLightInfo.OuterConeCos);
		c_sphere = InLightInfo.Position + InLightInfo.Direction * r_sphere;
	} else {
		r_sphere = InLightInfo.AttenuationRadius;
		c_sphere = InLightInfo.Position;
	}

	auto z_view = (c_sphere - CameraPos).Dot(Forward);
	auto r_ndc = (r_sphere / z_view) / tanf(FOV / 2.f);
	auto r_pixel = r_ndc * H / 2.f;
	auto A_screen = 3.1415925f * r_pixel * r_pixel;

	float desired_res = sqrtf(A_screen) * (Color.X * 0.2126f + Color.Y * 0.7152f + Color.Z * 0.0722f) * InLightInfo.Intensity;
	desired_res = desired_res > AtlasSize ? AtlasSize : desired_res;
	desired_res = static_cast<float>(RoundToNearestPowerOfTwo(static_cast<uint32>(desired_res)));
	return desired_res;
}