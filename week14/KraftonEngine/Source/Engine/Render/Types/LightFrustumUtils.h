#pragma once

#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Math/MathUtils.h"
#include "Collision/ConvexVolume.h"
#include "Render/Types/GlobalLightParams.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/ShadowSettings.h"
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
		float OrthoWidth = 0.0f;
		float OrthoHeight = 0.0f;
		float NearZ = 0.0f;
		float FarZ = 0.0f;
	};

	inline void ComputeOrthoWorldCorners(
		const FMatrix& View,
		float Width,
		float Height,
		float NearZ,
		float FarZ,
		FVector (&OutCorners)[8])
	{
		const float HalfWidth = Width * 0.5f;
		const float HalfHeight = Height * 0.5f;
		const FMatrix InvView = View.GetInverseFast();
		FVector Right = InvView.TransformVector(FVector(1.0f, 0.0f, 0.0f)).Normalized();
		FVector Up = InvView.TransformVector(FVector(0.0f, 1.0f, 0.0f)).Normalized();
		FVector Forward = InvView.TransformVector(FVector(0.0f, 0.0f, 1.0f)).Normalized();
		const float DepthLength = FarZ - NearZ;

		const FVector NearCenter = InvView.TransformPositionWithW(FVector(0.0f, 0.0f, NearZ));
		const FVector FarCenter = NearCenter + Forward * DepthLength;

		OutCorners[0] = NearCenter - Right * HalfWidth - Up * HalfHeight;
		OutCorners[1] = NearCenter + Right * HalfWidth - Up * HalfHeight;
		OutCorners[2] = NearCenter + Right * HalfWidth + Up * HalfHeight;
		OutCorners[3] = NearCenter - Right * HalfWidth + Up * HalfHeight;
		OutCorners[4] = FarCenter - Right * HalfWidth - Up * HalfHeight;
		OutCorners[5] = FarCenter + Right * HalfWidth - Up * HalfHeight;
		OutCorners[6] = FarCenter + Right * HalfWidth + Up * HalfHeight;
		OutCorners[7] = FarCenter - Right * HalfWidth + Up * HalfHeight;
	}

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
		Result.OrthoWidth = Width;
		Result.OrthoHeight = Height;
		Result.NearZ = NearZ;
		Result.FarZ = FarZ;
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

	// Receiver cascade slice 밖에 있지만 해당 slice에 그림자를 드리우는 caster를
	// 포함하기 위한 light-direction depth 길이. Ortho width/height는 유지되므로
	// shadow map의 X/Y texel density는 바뀌지 않는다.
	// 디폴트 값은 FShadowSettings::DirectionalShadowCasterDistance를 사용한다.
	inline constexpr float CSMShadowDepthLength = 500.0f;

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

			//로그 분할, logarithmic split, 원근 투영이라는 점을 반영
			float LogSplit = NearZ * powf(FarZ / NearZ, P);
			//선형 분할, Linear Splitm, 거리를 동일하게 간격 나눔
			float LinSplit = NearZ + (FarZ - NearZ) * P;
			//다시 둘을 interpolation함
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

	/**
	 * @brief Directional Light의 CSM cascade 하나에 대한 Light View/Projection 행렬을 생성한다.
	 *
	 * @param Light          Directional Light 정보. 주로 빛의 방향을 사용한다.
	 * @param CameraView     현재 카메라의 View 행렬.
	 * @param CameraProj     현재 카메라의 Projection 행렬.
	 * @param CameraNearZ    카메라 전체 near clip 거리.
	 * @param CameraFarZ     카메라 전체 far clip 거리.
	 * @param CascadeNearZ   현재 cascade가 담당하는 near 거리.
	 * @param CascadeFarZ    현재 cascade가 담당하는 far 거리.
	 *
	 * @return 현재 cascade용 directional light View/Projection 정보.
	 */
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

		// ------------------------------------------------------------
		// 1. 현재 cascade에 해당하는 카메라 절두체의 8개 코너를 월드 공간에서 구한다.
		//
		// 여기서 얻는 WorldCorners[8]은
		// "카메라가 보는 공간 중 현재 cascade가 담당하는 잘린 절두체"의 월드 좌표다.
		// ------------------------------------------------------------
		FVector WorldCorners[8];
		ComputeCascadeWorldCorners(
			CameraView, CameraProj,
			CameraNearZ, CameraFarZ,
			CascadeNearZ, CascadeFarZ,
			WorldCorners);

		// ------------------------------------------------------------
		// 2. cascade 절두체의 중심점을 구한다.
		//
		// 이 중심점은 directional light의 shadow camera가
		// 대략 어디를 바라봐야 하는지 정하는 기준점으로 사용된다.
		// ------------------------------------------------------------
		FVector Center(0, 0, 0);
		for (int i = 0; i < 8; ++i)
		{
			Center = Center + WorldCorners[i];
		}
		Center = Center * (1.0f / 8.0f);

		// ------------------------------------------------------------
		// 3. directional light의 방향과 view matrix용 Up 벡터를 구한다.
		//
		// SafeUpVector는 LightDir과 평행하지 않은 안정적인 Up 벡터를 고르는 함수다.
		// LookAt 행렬을 만들 때 forward와 up이 거의 평행하면 행렬이 불안정해지기 때문이다.
		// ------------------------------------------------------------
		FVector LightDir = Light.Direction.Normalized();
		FVector Up = SafeUpVector(LightDir);

		// ------------------------------------------------------------
		// 4. 임시 light view matrix를 만든다.
		//
		// 여기서는 cascade 중심점에서 LightDir의 반대 방향으로 100만큼 떨어진 곳에
		// 가상의 light camera를 놓고, Center를 바라보게 한다.
		//
		// LookAtLH(Eye, Target, Up)이므로,
		// Eye    = Center - LightDir * 100
		// Target = Center
		// 이면 view의 +Z 방향이 LightDir과 같아진다.
		// ------------------------------------------------------------
		Result.View = FMatrix::LookAtLH(Center - LightDir * 100.0f, Center, Up);

		// 5. light space에서 cascade 코너들을 감싸는 AABB를 구하기 위한 초기값.
		float MinX = FLT_MAX, MinY = FLT_MAX, MinZ = FLT_MAX;
		float MaxX = -FLT_MAX, MaxY = -FLT_MAX, MaxZ = -FLT_MAX;

		// ------------------------------------------------------------
		// 6. 월드 공간의 cascade 코너 8개를 light space로 변환한다.
		//
		// 변환된 좌표 LS의 의미:
		// - LS.X : 빛 기준 오른쪽/왼쪽 방향 위치
		// - LS.Y : 빛 기준 위/아래 방향 위치
		// - LS.Z : 빛이 바라보는 방향으로의 깊이
		//
		// 이 8개 점의 min/max를 구하면,
		// 현재 cascade 절두체를 빛의 시점에서 감싸는 박스를 얻을 수 있다.
		// ------------------------------------------------------------
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

		// ------------------------------------------------------------
		// 7. light space에서 cascade를 감싸는 orthographic 영역의 크기를 구한다.
		//
		// Width, Height는 shadow map을 찍을 orthographic projection의 가로/세로 크기다.
		//
		// 가까운 cascade는 보통 Width/Height가 작고,
		// 먼 cascade는 더 넓은 영역을 포함하므로 Width/Height가 커진다.
		// shadow map 해상도가 같다면 먼 cascade일수록 texel 하나가 담당하는 월드 공간이 커진다.
		// ------------------------------------------------------------
		float Width = MaxX - MinX;
		float Height = MaxY - MinY;

		// ------------------------------------------------------------
		// 8. light space에서 cascade 박스의 XY 중심을 구한다.
		//
		// 이 중심을 기준으로 shadow camera를 다시 정렬하면,
		// orthographic projection의 중앙에 cascade 영역이 오게 된다.
		// ------------------------------------------------------------
		float CenterX = (MinX + MaxX) * 0.5f;
		float CenterY = (MinY + MaxY) * 0.5f;

		// ------------------------------------------------------------
		// 9. Z 방향 깊이 범위를 고정 크기로 잡는다.
		//
		// MinZ ~ MaxZ만 딱 쓰면 cascade 절두체 자체는 감쌀 수 있지만,
		// 그림자를 드리우는 caster가 cascade 바깥쪽, 특히 빛 방향 앞쪽에 있을 수 있다.
		//
		// 그래서 CSMShadowDepthLength라는 고정된 깊이 범위를 사용해서
		// receiver 주변의 충분한 앞뒤 공간을 shadow map에 포함시키려는 것이다.
		//
		// ReceiverCenterZ는 현재 cascade receiver 영역의 중심 깊이다.
		// PaddedMinZ는 고정 깊이 범위의 시작점이다.
		// ------------------------------------------------------------
		const float ReceiverCenterZ = (MinZ + MaxZ) * 0.5f;
		const float PaddedDepthRange = FShadowSettings::Get().GetEffectiveCSMCasterDistance();
		const float PaddedMinZ = ReceiverCenterZ - PaddedDepthRange * 0.5f;

		// 10. 현재 임시 light view의 역행렬을 구한다.
		FMatrix InvLightView = Result.View.GetInverseFast();

		// ------------------------------------------------------------
		// 11. 최종 shadow camera의 월드 공간 위치를 계산한다.
		//
		// LSCenter는 light space에서의 최종 shadow camera 위치다.
		//
		// X, Y는 cascade 박스의 중심에 맞춘다.
		// Z는 PaddedMinZ로 둔다.
		//
		// 즉, 최종 light camera를
		// "cascade를 XY 중앙에 두고, Z 방향으로는 충분히 앞쪽에서 시작하는 위치"에 놓는다.
		// ------------------------------------------------------------
		FVector LSCenter(CenterX, CenterY, PaddedMinZ);
		FVector WSCenter = InvLightView.TransformPositionWithW(LSCenter);

		// ------------------------------------------------------------
		// 12. 최종 light view matrix를 다시 만든다.
		//
		// Target = WSCenter + LightDir 이므로,
		// 최종 shadow camera는 LightDir 방향을 바라본다.
		//
		// 이 view matrix가 실제 shadow map을 렌더링할 때 사용되는 light view다.
		// ------------------------------------------------------------
		Result.View = FMatrix::LookAtLH(
			WSCenter,
			WSCenter + LightDir,
			Up
		);

		// ------------------------------------------------------------
		// 13. orthographic projection 파라미터를 저장한다.
		//
		// 여기서는 light camera의 위치 자체를 PaddedMinZ에 맞췄기 때문에,
		// NearZ는 0부터 시작하고 FarZ는 PaddedDepthRange가 된다.
		// ------------------------------------------------------------
		Result.OrthoWidth = Width;
		Result.OrthoHeight = Height;
		Result.NearZ = 0.0f;
		Result.FarZ = PaddedDepthRange;

		// 14. 최종 orthographic projection matrix를 만든다.
		Result.Proj = FMatrix::OrthoLH(
			Width,
			Height,
			Result.NearZ,
			Result.FarZ
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
