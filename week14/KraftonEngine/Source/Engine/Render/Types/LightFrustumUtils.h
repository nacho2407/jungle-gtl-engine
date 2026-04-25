#pragma once

#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Collision/ConvexVolume.h"
#include "Render/Types/GlobalLightParams.h"
#include "Render/Types/FrameContext.h"

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
	inline void BuildPointLightFaceViewProj(
		const FPointLightParams& Light,
		FPointLightFaceViewProj OutFaces[6],
		float NearZ = 0.1f)
	{
		static const FVector FaceDirs[6] = {
			FVector( 1,  0,  0),  // +X
			FVector(-1,  0,  0),  // -X
			FVector( 0,  1,  0),  // +Y
			FVector( 0, -1,  0),  // -Y
			FVector( 0,  0,  1),  // +Z
			FVector( 0,  0, -1),  // -Z
		};
		static const FVector FaceUps[6] = {
			FVector(0, 0, 1),  // +X
			FVector(0, 0, 1),  // -X
			FVector(0, 0, 1),  // +Y
			FVector(0, 0, 1),  // -Y
			FVector(0, 1, 0),  // +Z (up = +Y to avoid parallel)
			FVector(0, 1, 0),  // -Z
		};

		// 90° FOV, 1:1 aspect
		FMatrix Proj = FMatrix::PerspectiveFovLH(
			3.14159265358979f * 0.5f, 1.0f, NearZ, Light.AttenuationRadius
		);

		for (int i = 0; i < 6; ++i)
		{
			OutFaces[i].View = FMatrix::LookAtLH(
				Light.Position,
				Light.Position + FaceDirs[i],
				FaceUps[i]
			);
			OutFaces[i].Proj = Proj;
			OutFaces[i].ViewProj = OutFaces[i].View * Proj;
		}
	}

	inline void BuildPointLightFaceFrustums(
		const FPointLightParams& Light,
		FConvexVolume OutFrustums[6],
		float NearZ = 0.1f)
	{
		FPointLightFaceViewProj Faces[6];
		BuildPointLightFaceViewProj(Light, Faces, NearZ);

		for (int i = 0; i < 6; ++i)
			OutFrustums[i].UpdateFromMatrix(Faces[i].ViewProj);
	}

	// ============================================================
	// Directional Light — 카메라 frustum 기반 orthographic shadow
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
		const FMatrix& CameraProj)
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
	// Ambient Light — frustum 없음 (전방향 균일 조명)
	// ============================================================
	// Ambient는 방향/위치가 없으므로 frustum culling 대상이 아님.
	// 완전성을 위해 stub 함수만 제공.
	inline bool HasFrustum(const FGlobalAmbientLightParams& /*Light*/) { return false; }
}
