#include "Engine/Render/Culling/OcclusionCulling.h"
#include "Engine/Core/EngineTypes.h"
#include <algorithm>

void FOcclusionCulling::Clear()
{
	std::fill(std::begin(DepthBuffer), std::end(DepthBuffer), 1.0f);
}

void FOcclusionCulling::RasterizeOccluder(const FBoundingBox& Box, const FMatrix& ViewProj)
{
	FVector Center = Box.GetCenter();
	FVector Corners[8];
	Box.GetCorners(Corners);

	const float ShrinkFactor = 0.7f;
	for (int i = 0; i < 8; ++i)
	{
		// 중심 방향으로 이동시켜 박스 크기를 줄임
		Corners[i] = Center + (Corners[i] - Center) * ShrinkFactor;
	}

	// -> NDC
	float MinX = 1, MaxX = -1, MinY = 1, MaxY = -1;
	float MaxZ = -1.0f;
	for (auto& C : Corners) {
		FVector4 Clip = ViewProj.TransformPositionWithW(C);
		if (Clip.W <= 0.1f) return;
		float InvW = 1.f / Clip.W;
		float Nx = Clip.X * InvW;
		float Ny = Clip.Y * InvW;
		float Nz = Clip.Z * InvW;
		MinX = std::min(MinX, Nx); MaxX = std::max(MaxX, Nx);
		MinY = std::min(MinY, Ny); MaxY = std::max(MaxY, Ny);
		MaxZ = std::max(MaxZ, Nz);
	}

	// NDC → 버퍼 픽셀 좌표
	int X0 = (int)((MinX * 0.5f + 0.5f) * BUF_W);
	int X1 = (int)((MaxX * 0.5f + 0.5f) * BUF_W);
	int Y0 = (int)((1.0f - MaxY) * 0.5f * BUF_H);
	int Y1 = (int)((1.0f - MinY) * 0.5f * BUF_H);
	X0 = std::clamp(X0, 0, BUF_W - 1);
	X1 = std::clamp(X1, 0, BUF_W - 1);
	Y0 = std::clamp(Y0, 0, BUF_H - 1);
	Y1 = std::clamp(Y1, 0, BUF_H - 1);

	//for (int Y = Y0; Y <= Y1; ++Y)
	//	for (int X = X0; X <= X1; ++X)
	//		DepthBuffer[Y * BUF_W + X] = std::min(DepthBuffer[Y * BUF_W + X], MaxZ);
	
	// SIMD ver.
	__m256 maxZVec = _mm256_set1_ps(MaxZ);

	for (int Y = Y0; Y <= Y1; ++Y)
	{
		int RowOffset = Y * BUF_W;

		int X = X0;

		// 8개씩 처리 (AVX2)
		for (; X <= X1 - 7; X += 8)
		{
			__m256 old = _mm256_loadu_ps(&DepthBuffer[RowOffset + X]);
			__m256 newv = _mm256_min_ps(old, maxZVec);
			_mm256_storeu_ps(&DepthBuffer[RowOffset + X], newv);
		}

		// 남은 픽셀 처리 (tail)
		for (; X <= X1; ++X)
		{
			DepthBuffer[RowOffset + X] =
				std::min(DepthBuffer[RowOffset + X], MaxZ);
		}
	}
}

bool FOcclusionCulling::IsOccluded(const FBoundingBox& Box, const FMatrix& ViewProj)
{
	FVector Corners[8];
	Box.GetCorners(Corners);

	float MinX = 1.f, MaxX = -1.f, MinY = 1.f, MaxY = -1.f, MinZ = 1.f;

	for (auto& C : Corners) {
		FVector4 Clip = ViewProj.TransformPositionWithW(C);
		if (Clip.W <= 0.1f) return false;
		float InvW = 1.f / Clip.W;
		float Nx = Clip.X * InvW;
		float Ny = Clip.Y * InvW;
		float Nz = Clip.Z * InvW;
		MinX = std::min(MinX, Nx); MaxX = std::max(MaxX, Nx);
		MinY = std::min(MinY, Ny); MaxY = std::max(MaxY, Ny);
		MinZ = std::min(MinZ, Nz);
	}

	int X0 = (int)((MinX * 0.5f + 0.5f) * BUF_W);
	int X1 = (int)((MaxX * 0.5f + 0.5f) * BUF_W);
	int Y0 = (int)((1.0f - MaxY) * 0.5f * BUF_H);
	int Y1 = (int)((1.0f - MinY) * 0.5f * BUF_H);
	X0 = std::clamp(X0, 0, BUF_W - 1);
	X1 = std::clamp(X1, 0, BUF_W - 1);
	Y0 = std::clamp(Y0, 0, BUF_H - 1);
	Y1 = std::clamp(Y1, 0, BUF_H - 1);

	// AABB가 덮는 모든 픽셀에서 Depth Buffer보다 뒤에 있으면 가려진 것
	//const float DepthBias = 0.001f;
	//for (int Y = Y0; Y <= Y1; ++Y)
	//{
	//	int RowOffset = Y * BUF_W;
	//	for (int X = X0; X <= X1; ++X)
	//		if (MinZ - DepthBias <= DepthBuffer[RowOffset + X])
	//			return false;
	//}

	// SIMD ver.
	const float DepthBias = 0.001f;
	for (int Y = Y0; Y <= Y1; ++Y)
	{
		int RowOffset = Y * BUF_W;
		__m256 minZVec = _mm256_set1_ps(MinZ - DepthBias);

		for (int X = X0; X <= X1; X += 8)
		{
			__m256 depth = _mm256_loadu_ps(&DepthBuffer[RowOffset + X]);
			__m256 cmp = _mm256_cmp_ps(minZVec, depth, _CMP_LE_OQ);

			int mask = _mm256_movemask_ps(cmp);
			if (mask != 0)
				return false;
		}
	}

	return true; 
}