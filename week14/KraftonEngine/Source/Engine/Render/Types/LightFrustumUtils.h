#pragma once

#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Math/MathUtils.h"
#include "Collision/ConvexVolume.h"
#include "Render/Types/GlobalLightParams.h"
#include "Render/Types/FrameContext.h"
#include <numbers>

/*
	FLightFrustumUtils — 라이트 타입별 View/Projection 행렬 및 FConvexVolume 생성 유틸리티.
	ShadowMapPass, Light Culling 등에서 per-light frustum culling에 사용.
*/
namespace FLightFrustumUtils
{
	// ============================================================
	// Up 벡터 안전 선택 — Direction과 평행하지 않은 Up 반환
	// ============================================================
	inline FVector SafeUpVector(const FVector& Direction)
	{
		FVector Up(0.0f, 0.0f, 1.0f);
		if (fabsf(Direction.Dot(Up)) > 0.99f)
			Up = FVector(1.0f, 0.0f, 0.0f);
		return Up;
	}

	// ============================================================
	// Spot Light
	// ============================================================
	struct FSpotLightViewProj
	{
		FMatrix View;
		FMatrix Proj;
		FMatrix ViewProj;
	};

	inline FSpotLightViewProj BuildSpotLightViewProj(const FSpotLightParams& Light, float NearZ = 0.1f)
	{
		FSpotLightViewProj Result;

		FVector Up = SafeUpVector(Light.Direction);
		Result.View = FMatrix::LookAtLH(Light.Position, Light.Position + Light.Direction, Up);

		float FovY = acosf(Light.OuterConeCos) * 2.0f;
		Result.Proj = FMatrix::PerspectiveFovLH(FovY, 1.0f, NearZ, Light.AttenuationRadius);

		Result.ViewProj = Result.View * Result.Proj;
		return Result;
	}

	inline FConvexVolume BuildSpotLightFrustum(const FSpotLightParams& Light, float NearZ = 0.1f)
	{
		FConvexVolume Volume;
		Volume.UpdateFromMatrix(BuildSpotLightViewProj(Light, NearZ).ViewProj);
		return Volume;
	}

	// ============================================================
	// Point Light — 6면 큐브맵 face별 ViewProj
	// ============================================================
	struct FPointLightFaceViewProj
	{
		FMatrix View;
		FMatrix Proj;
		FMatrix ViewProj;
	};

	// 큐브맵 6개 face 방향 (+X, -X, +Y, -Y, +Z, -Z)
	inline FPointLightFaceViewProj BuildPointLightFaceViewProj(
		const FPointLightParams& Light,
		const uint32 FaceIndex,
		float NearZ = 0.1f)
	{
		static const TStaticArray<FVector, 6> FaceDirections = {
			FVector(1.0f , 0.0f, 0.0f),
			FVector(-1.0f, 0.0f, 0.0f),
			FVector(0.0f, 1.0f, 0.0f),
			FVector(0.0f, -1.0f, 0.0f),
			FVector(0.0f, 0.0f, 1.0f),
			FVector(0.0f, 0.0f, -1.0f),
		};

		static const TStaticArray<FVector, 6> FaceUps = {
			FVector(0.0f, 0.0f, 1.0f),
			FVector(0.0f, 0.0f, 1.0f),
			FVector(0.0f, 0.0f, 1.0f),
			FVector(0.0f, 0.0f, 1.0f),
			FVector(0.0f, 1.0f, 0.0f),
			FVector(0.0f, 1.0f, 0.0f),
		};

		FPointLightFaceViewProj Result;

		Result.View = FMatrix::LookAtLH(
			Light.Position,
			Light.Position + FaceDirections[FaceIndex],
			FaceUps[FaceIndex]);
		Result.Proj = FMatrix::PerspectiveFovLH(
			std::numbers::pi_v<float> * 0.5f, 1.0f, NearZ, Light.AttenuationRadius
		);
		Result.ViewProj = Result.View * Result.Proj;
		return Result;
	}

	inline FConvexVolume BuildPointLightFaceFrustums(
		const FPointLightParams& Light,
		const uint32 FaceIndex,
		float NearZ = 0.1f)
	{
		FConvexVolume Volume;
		Volume.UpdateFromMatrix(BuildPointLightFaceViewProj(Light, FaceIndex, NearZ).ViewProj);
		return Volume;
	}

	// ============================================================
	// Directional Light(1) — 카메라 frustum 기반 orthographic shadow
	// ============================================================
	struct FDirectionalLightViewProj
	{
		FMatrix View;
		FMatrix Proj;
		FMatrix ViewProj;
	};

	// CameraView/CameraProj로 카메라 frustum 8개 꼭짓점을 구하고,
	// Light 방향의 직교 투영으로 감싸는 행렬을 생성.
		inline FDirectionalLightViewProj BuildDirectionalLightViewProj(
			const FGlobalDirectionalLightParams& Light,
			const FMatrix& CameraView,
			const FMatrix& CameraProj
			// CSM이 아닌 single shadow map에서 카메라 far clip 전체를 감싸면 ortho 범위가 너무 커져
			// shadow texel 밀도가 낮아집니다. 우선 카메라 주변 일정 거리만 덮습니다.
			//float ShadowDistance = 100.0f
			)
	{
		FDirectionalLightViewProj Result;

		// 카메라 ViewProj 역행렬로 NDC 코너를 월드로 변환
		FMatrix InvVP = (CameraView * CameraProj).GetInverse();

		// NDC 8개 꼭짓점 (Reversed-Z: near=1, far=0)
		static const FVector NDCCorners[8] = {
			FVector(-1, -1, 1), FVector( 1, -1, 1), FVector( 1,  1, 1), FVector(-1,  1, 1), // near
			FVector(-1, -1, 0), FVector( 1, -1, 0), FVector( 1,  1, 0), FVector(-1,  1, 0), // far
		};

		FVector WorldCorners[8];
		for (int i = 0; i < 8; ++i)
			WorldCorners[i] = InvVP.TransformPositionWithW(NDCCorners[i]);

		//if (ShadowDistance > 0.0f)
		//{
		//	FMatrix InvView = CameraView.GetInverseFast();
		//	FVector CameraPos = InvView.TransformPositionWithW(FVector(0.0f, 0.0f, 0.0f));
		//	for (int i = 4; i < 8; ++i)
		//	{
		//		FVector ToCorner = WorldCorners[i] - CameraPos;
		//		float Dist = ToCorner.Length();
		//		if (Dist > ShadowDistance && Dist > 0.001f)
		//		{
		//			WorldCorners[i] = CameraPos + ToCorner * (ShadowDistance / Dist);
		//		}
		//	}
		//}

		// Frustum 중심
		FVector Center(0, 0, 0);
		for (int i = 0; i < 8; ++i)
			Center = Center + WorldCorners[i];
		Center = Center * (1.0f / 8.0f);

		// Light View 행렬
		FVector LightDir = Light.Direction.Normalized();
		FVector Up = SafeUpVector(LightDir);
		Result.View = FMatrix::LookAtLH(Center - LightDir * 100.0f, Center, Up);

		// Light space에서 frustum AABB 계산
		float MinX =  FLT_MAX, MinY =  FLT_MAX, MinZ =  FLT_MAX;
		float MaxX = -FLT_MAX, MaxY = -FLT_MAX, MaxZ = -FLT_MAX;

		for (int i = 0; i < 8; ++i)
		{
			FVector LS = Result.View.TransformPositionWithW(WorldCorners[i]);
			if (LS.X < MinX) MinX = LS.X;
			if (LS.X > MaxX) MaxX = LS.X;
			if (LS.Y < MinY) MinY = LS.Y;
			if (LS.Y > MaxY) MaxY = LS.Y;
			if (LS.Z < MinZ) MinZ = LS.Z;
			if (LS.Z > MaxZ) MaxZ = LS.Z;
		}

		float Width  = MaxX - MinX;
		float Height = MaxY - MinY;

		// View를 AABB 중심으로 재조정
		float CenterX = (MinX + MaxX) * 0.5f;
		float CenterY = (MinY + MaxY) * 0.5f;

		// Light space AABB 중심을 월드 좌표로 역변환 후 View 재생성
		FMatrix InvView = Result.View.GetInverseFast();
		FVector LSCenter(CenterX, CenterY, MinZ);
		FVector WSCenter = InvView.TransformPositionWithW(LSCenter);

		Result.View = FMatrix::LookAtLH(WSCenter - LightDir * (MaxZ - MinZ), WSCenter, Up);

		// 넉넉한 depth range
		float NearZ = 0.0f;
		float FarZ  = (MaxZ - MinZ) + 100.0f;
		Result.Proj = FMatrix::OrthoLH(Width, Height, NearZ, FarZ);

		Result.ViewProj = Result.View * Result.Proj;
		return Result;
	}

	inline FConvexVolume BuildDirectionalLightFrustum(
		const FGlobalDirectionalLightParams& Light,
		const FMatrix& CameraView,
		const FMatrix& CameraProj)
	{
		FConvexVolume Volume;
		Volume.UpdateFromMatrix(
			BuildDirectionalLightViewProj(Light, CameraView, CameraProj).ViewProj
		);
		return Volume;
	}

	// ============================================================
	// Directional Light(2) — 카메라 frustum 기반 Cascaded Shadow Map
	// ============================================================

	struct FCascadeRange
	{
		float NearZ;
		float FarZ;
	};

	inline void ComputeCascadeRanges(
		float NearZ,
		float FarZ,
		int32 NumCascades,
		float Lambda,
		FCascadeRange* OutRanges)
	{
		Lambda = Clamp(Lambda, 0.0f, 1.0f);

		float Prev = NearZ;

		for (int32 i = 0; i < NumCascades; ++i)
		{
			float P = static_cast<float>(i + 1) / static_cast<float>(NumCascades);

			float LogSplit = NearZ * powf(FarZ / NearZ, P);
			float LinSplit = NearZ + (FarZ - NearZ) * P;
			float Split = LinSplit * (1.0f - Lambda) + LogSplit * Lambda;

			OutRanges[i].NearZ = Prev;
			OutRanges[i].FarZ = Split;

			Prev = Split;
		}

		OutRanges[NumCascades - 1].FarZ = FarZ;
	}

	inline void ComputeCascadeWorldCorners(
		const FMatrix& CameraView,
		const FMatrix& CameraProj,
		float CameraNearZ,
		float CameraFarZ,
		float CascadeNearZ,
		float CascadeFarZ,
		FVector (&OutCorners)[8])
	{
		FMatrix InvVP = (CameraView * CameraProj).GetInverse();

		static const FVector NDCCorners[8] = {
			FVector(-1, -1, 1), FVector(1, -1, 1), FVector(1,  1, 1), FVector(-1,  1, 1),
			FVector(-1, -1, 0), FVector(1, -1, 0), FVector(1,  1, 0), FVector(-1,  1, 0),
		};

		FVector FullCorners[8];
		for (int i = 0; i < 8; ++i)
		{
			FullCorners[i] = InvVP.TransformPositionWithW(NDCCorners[i]);
		}

		float NearT = (CascadeNearZ - CameraNearZ) / (CameraFarZ - CameraNearZ);
		float FarT = (CascadeFarZ - CameraNearZ) / (CameraFarZ - CameraNearZ);

		NearT = Clamp(NearT, 0.0f, 1.0f);
		FarT = Clamp(FarT, 0.0f, 1.0f);

		for (int i = 0; i < 4; ++i)
		{
			const FVector& FullNear = FullCorners[i];
			const FVector& FullFar = FullCorners[i + 4];

			OutCorners[i] = FullNear + (FullFar - FullNear) * NearT;
			OutCorners[i + 4] = FullNear + (FullFar - FullNear) * FarT;
		}
	}

	inline FDirectionalLightViewProj BuildDirectionalLightCascadeViewProj(
		const FGlobalDirectionalLightParams& Light,
		const FMatrix& CameraView,
		const FMatrix& CameraProj,
		float CameraNearZ,
		float CameraFarZ,
		float CascadeNearZ,
		float CascadeFarZ)
	{
		FDirectionalLightViewProj Result;

		FVector WorldCorners[8];
		ComputeCascadeWorldCorners(
			CameraView, CameraProj,
			CameraNearZ, CameraFarZ,
			CascadeNearZ, CascadeFarZ,
			WorldCorners);

		FVector Center(0, 0, 0);
		for (int i = 0; i < 8; ++i)
		{
			Center = Center + WorldCorners[i];
		}
		Center = Center * (1.0f / 8.0f);

		FVector LightDir = Light.Direction.Normalized();
		FVector Up = SafeUpVector(LightDir);

		Result.View = FMatrix::LookAtLH(Center - LightDir * 100.0f, Center, Up);

		float MinX = FLT_MAX, MinY = FLT_MAX, MinZ = FLT_MAX;
		float MaxX = -FLT_MAX, MaxY = -FLT_MAX, MaxZ = -FLT_MAX;

		for (int i = 0; i < 8; ++i)
		{
			FVector LS = Result.View.TransformPositionWithW(WorldCorners[i]);

			MinX = (std::min)(MinX, LS.X);
			MaxX = (std::max)(MaxX, LS.X);
			MinY = (std::min)(MinY, LS.Y);
			MaxY = (std::max)(MaxY, LS.Y);
			MinZ = (std::min)(MinZ, LS.Z);
			MaxZ = (std::max)(MaxZ, LS.Z);
		}

		float Width = MaxX - MinX;
		float Height = MaxY - MinY;

		float CenterX = (MinX + MaxX) * 0.5f;
		float CenterY = (MinY + MaxY) * 0.5f;

		FMatrix InvLightView = Result.View.GetInverseFast();

		FVector LSCenter(CenterX, CenterY, MinZ);
		FVector WSCenter = InvLightView.TransformPositionWithW(LSCenter);

		float DepthRange = MaxZ - MinZ;

		Result.View = FMatrix::LookAtLH(
			WSCenter - LightDir * DepthRange,
			WSCenter,
			Up
		);

		const float DepthPadding = 100.0f;

		Result.Proj = FMatrix::OrthoLH(
			Width,
			Height,
			0.0f,
			DepthRange + DepthPadding
		);

		Result.ViewProj = Result.View * Result.Proj;
		return Result;
	}


	// ============================================================
	// Ambient Light — frustum 없음 (전방향 균일 조명)
	// ============================================================
	// Ambient는 방향/위치가 없으므로 frustum culling 대상이 아님.
	// 완전성을 위해 stub 함수만 제공.
	inline bool HasFrustum(const FGlobalAmbientLightParams& /*Light*/) { return false; }
}
