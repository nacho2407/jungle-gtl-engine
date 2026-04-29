#include "AtlasQuadTreePoint.h"
#include "Render/Types/GlobalLightParams.h"

#include <algorithm>

void FAtlasQuadTreePoint::AddToBatch(const FPointLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H, int32 LightIdx) {
	Batch.push_back({ InLightInfo, EvaluateResolution(InLightInfo, CameraPos, Forward, FOV, H), LightIdx });
}

void FAtlasQuadTreePoint::AddToBatch(const FPointLightParams& InLightInfo, float OverrideResolution, int32 LightIdx) {
	Batch.push_back({ InLightInfo, OverrideResolution, LightIdx });
}

uint32 FAtlasQuadTreePoint::ComputeSnappedResolution(const FPointLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H) const {
	float res = EvaluateResolution(InLightInfo, CameraPos, Forward, FOV, H);
	// AtlasSize/2 캡: QuadTree의 >= 가드가 AtlasSize 요청을 절반으로 내리므로 추정과 실제 일치시킴
	uint32 maxRes = static_cast<uint32>(AtlasSize) / 2;
	uint32 snapped = RoundToNearestPowerOfTwo(static_cast<uint32>((std::min)(res, static_cast<float>(maxRes))));
	if (snapped < static_cast<uint32>(MinShadowMapResolution)) snapped = static_cast<uint32>(MinShadowMapResolution);
	if (snapped > maxRes) snapped = maxRes;
	return snapped;
}

TArray<FAtlasRegion> FAtlasQuadTreePoint::CommitBatch() {
	const int32 N = static_cast<int32>(Batch.size());

	// Results are written back at original indices.
	TArray<int32> Order(N);
	for (int32 i = 0; i < N; ++i) Order[i] = i;
	std::sort(Order.begin(), Order.end(), [&](int32 A, int32 B) {
		return Batch[A].Resolution > Batch[B].Resolution;
		});

	TArray<FAtlasRegion> Results(N, { 0, 0, 0, false, -1 });
	for (int32 OrigIdx : Order) {
		// Clamp and snap to nearest power of 2
		auto desired_res = std::min(Batch[OrigIdx].Resolution, AtlasSize);
		desired_res = static_cast<float>(RoundToNearestPowerOfTwo(static_cast<uint32>(desired_res)));
		desired_res = desired_res > MinShadowMapResolution ? desired_res : MinShadowMapResolution;
		FAtlasRegion AtlasRegion = AllocateNode(0, desired_res, Batch[OrigIdx].LightIdx, Batch[OrigIdx].Light.CubeMapOrientation);
		Results[OrigIdx] = AtlasRegion;
	}

	Batch.clear();
	return Results;
}

float FAtlasQuadTreePoint::EvaluateResolution(const FPointLightParams& InLightInfo, FVector CameraPos, FVector Forward, float FOV, float H) const {
	if (InLightInfo.bCastShadows == false) return 0.f;
	FVector4 Color = InLightInfo.LightColor;

	// Bounding Sphere
	float r_sphere = InLightInfo.AttenuationRadius;
	FVector c_sphere = InLightInfo.Position;

	// Orthogonal view-space depth
	auto z_view = (c_sphere - CameraPos).Dot(Forward);
	z_view = z_view > z_guard ? z_view : z_guard;

	// Project to screen space
	auto r_ndc = (r_sphere / z_view) / tanf(FOV / 2.f);
	auto r_pixel = r_ndc * H / 2.f;
	auto A_screen = 3.14159265f * r_pixel * r_pixel;

	// Calculate ideal resolution based on area, luminance, and intensity
	float ResolutionScale = InLightInfo.ShadowResolutionScale;
	float desired_res = sqrtf(A_screen) * (Color.X * 0.2126f + Color.Y * 0.7152f + Color.Z * 0.0722f) * InLightInfo.Intensity / (InLightInfo.LightFalloffExponent) / 3 * ResolutionScale;

	return desired_res;
}