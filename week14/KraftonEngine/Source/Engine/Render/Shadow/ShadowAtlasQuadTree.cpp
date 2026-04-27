#include "ShadowAtlasQuadTree.h"
#include "Render/Types/GlobalLightParams.h"

#include <algorithm>

// Public functions

// Unused for now
//FAtlasRegion FShadowAtlasQuadTree::Add(const FSpotLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H) {
//	if (Nodes.empty()) return { 0, 0, 0, false };
//	float RequestedSize = EvaluateResolution(InLightInfo, CameraPos, Forward, FOV, H);
//	return AllocateNode(0, RequestedSize);
//}

void FShadowAtlasQuadTree::AddToBatch(const FSpotLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H) {
	Batch.push_back({InLightInfo, EvaluateResolution(InLightInfo, CameraPos, Forward, FOV, H)});
}

float FShadowAtlasQuadTree::EvaluateResolution(const FSpotLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H) const {
	if (InLightInfo.bCastShadows == false) return 0.f;

	FVector4 Color		 = InLightInfo.LightColor;
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
	z_view = z_view > z_guard ? z_view : z_guard;
	auto r_ndc = (r_sphere / z_view) / tanf(FOV / 2.f);
	auto r_pixel = r_ndc * H / 2.f;
	auto A_screen = 3.1415925f * r_pixel * r_pixel;

	return sqrtf(A_screen) * (Color.X * 0.2126f + Color.Y * 0.7152f + Color.Z * 0.0722f) * InLightInfo.Intensity * InLightInfo.ShadowResolutionScale;
}

TArray<FAtlasRegion> FShadowAtlasQuadTree::CommitBatch() {
	const int32 N = static_cast<int32>(Batch.size());
	
	// Results are written back at original indices.
	TArray<int32> Order(N);
	for (int32 i = 0; i < N; ++i) Order[i] = i;
	std::sort(Order.begin(), Order.end(), [&](int32 A, int32 B) {
		return Batch[A].second > Batch[B].second;
	});

	TArray<FAtlasRegion> Results(N, { 0, 0, 0, false });
	for (int32 OrigIdx : Order) {
		auto desired_res = static_cast<float>(RoundToNearestPowerOfTwo(static_cast<uint32>(Batch[OrigIdx].second)));
		FAtlasRegion AtlasRegion = AllocateNode(0, desired_res);
		Results[OrigIdx] = AtlasRegion;
	}

	Batch.clear();
	return Results;
}