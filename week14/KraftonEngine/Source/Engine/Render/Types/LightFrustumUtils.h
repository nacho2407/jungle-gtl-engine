#pragma once

#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Math/MathUtils.h"
#include "Core/Types/EngineTypes.h"
#include "Collision/Math/ConvexVolume.h"
#include "Render/Types/GlobalLightParams.h"
#include "Render/Types/FrameContext.h"
#include "Render/Types/ShadowSettings.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
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
		float DebugReceiverMinZ = 0.0f;
		float DebugReceiverMaxZ = 0.0f;
		float DebugFinalMinZ = 0.0f;
		float DebugFinalMaxZ = 0.0f;
		uint32 DebugCandidateCasterCount = 0;
		uint32 DebugIncludedCasterCount = 0;
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

	// Stable CSM용 receiver 범위 패딩.
	// tight-fit AABB 대신 bounding sphere를 사용해서 카메라 회전 중 ortho 크기가 흔들리지 않게 한다.
	inline constexpr float CSMStableExtentPadding = 1.05f;
	inline constexpr float CSMStableExtentQuantization = 1.0f / 16.0f;
	inline constexpr float CSMMinStableOrthoSize = 1.0f;
	inline constexpr float CSMReceiverDepthPadding = 100.0f;

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

	/** @brief 지정 값을 step 단위로 올림 정렬한다 */
	inline float RoundUpToStep(float Value, float Step)
	{
		if (Step <= 0.0f)
		{
			return Value;
		}

		return std::ceil(Value / Step) * Step;
	}

	/** @brief 지정 값을 shadow texel grid에 맞춰 반올림한다 */
	inline float SnapToTexelGrid(float Value, float TexelSize)
	{
		if (TexelSize <= 0.000001f)
		{
			return Value;
		}

		return std::floor(Value / TexelSize + 0.5f) * TexelSize;
	}

	/**
	 * @brief Light-space AABB 계산 결과
	 */
	struct FLightSpaceBounds
	{
		float MinX = FLT_MAX;
		float MinY = FLT_MAX;
		float MinZ = FLT_MAX;
		float MaxX = -FLT_MAX;
		float MaxY = -FLT_MAX;
		float MaxZ = -FLT_MAX;
	};

	/** @brief Light-space bounds에 점 하나를 포함시킨다 */
	inline void IncludeLightSpacePoint(FLightSpaceBounds& Bounds, const FVector& Point)
	{
		Bounds.MinX = (std::min)(Bounds.MinX, Point.X);
		Bounds.MinY = (std::min)(Bounds.MinY, Point.Y);
		Bounds.MinZ = (std::min)(Bounds.MinZ, Point.Z);
		Bounds.MaxX = (std::max)(Bounds.MaxX, Point.X);
		Bounds.MaxY = (std::max)(Bounds.MaxY, Point.Y);
		Bounds.MaxZ = (std::max)(Bounds.MaxZ, Point.Z);
	}

	/** @brief 월드 AABB를 light-space AABB로 변환한다 */
	inline FLightSpaceBounds BuildLightSpaceBounds(const FMatrix& LightRotationView, const FBoundingBox& WorldBounds)
	{
		FLightSpaceBounds Result;
		FVector Corners[8];
		WorldBounds.GetCorners(Corners);

		for (const FVector& Corner : Corners)
		{
			IncludeLightSpacePoint(Result, LightRotationView.TransformPositionWithW(Corner));
		}

		return Result;
	}

	/** @brief 두 light-space AABB가 XY 평면에서 겹치는지 확인한다 */
	inline bool OverlapsLightSpaceXY(
		const FLightSpaceBounds& Bounds,
		float MinX,
		float MinY,
		float MaxX,
		float MaxY,
		float Padding)
	{
		return Bounds.MaxX >= MinX - Padding
			&& Bounds.MinX <= MaxX + Padding
			&& Bounds.MaxY >= MinY - Padding
			&& Bounds.MinY <= MaxY + Padding;
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
	 * @param ShadowResolution 현재 cascade shadow map 해상도.
	 * @param CasterBounds   shadow caster 후보들의 월드 AABB 배열.
	 * @param CasterBoundsCount shadow caster 후보 개수.
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
		float CascadeFarZ,
		uint32 ShadowResolution = FShadowSettings::kDefaultCSMResolution,
		const FBoundingBox* CasterBounds = nullptr,
		uint32 CasterBoundsCount = 0)
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
		// 2. cascade 절두체의 중심점과 bounding sphere 반지름을 구한다.
		//
		// 기존 방식은 light-space AABB의 Width/Height를 매 프레임 그대로 사용했다.
		// 그러면 카메라가 제자리에서 회전만 해도 AABB 크기가 달라져 shadow projection이 흔들린다.
		// bounding sphere 기반의 정사각 ortho 영역은 회전에 덜 민감해서 temporal stability가 좋아진다.
		// ------------------------------------------------------------
		FVector Center(0, 0, 0);
		for (int i = 0; i < 8; ++i)
		{
			Center = Center + WorldCorners[i];
		}
		Center = Center * (1.0f / 8.0f);

		float Radius = 0.0f;
		for (int i = 0; i < 8; ++i)
		{
			Radius = (std::max)(Radius, (WorldCorners[i] - Center).Length());
		}

		const float RawOrthoSize = (std::max)(
			Radius * 2.0f * CSMStableExtentPadding,
			CSMMinStableOrthoSize);
		const float OrthoSize = RoundUpToStep(RawOrthoSize, CSMStableExtentQuantization);
		const float TexelSize = OrthoSize / static_cast<float>((std::max)(ShadowResolution, 1u));

		// ------------------------------------------------------------
		// 3. directional light의 방향과 view matrix용 Up 벡터를 구한다.
		//
		// SafeUpVector는 LightDir과 평행하지 않은 안정적인 Up 벡터를 고르는 함수다.
		// LookAt 행렬을 만들 때 forward와 up이 거의 평행하면 행렬이 불안정해지기 때문이다.
		// ------------------------------------------------------------
		FVector LightDir = Light.Direction.Normalized();
		FVector Up = SafeUpVector(LightDir);

		// ------------------------------------------------------------
		// 4. translation 없는 light rotation view를 만든다.
		//
		// texel snapping은 월드 기준 light-space grid에 맞춰야 의미가 있다.
		// Center를 Eye로 삼은 임시 view를 쓰면 중심이 항상 0이 되므로 snapping 효과가 사라진다.
		// ------------------------------------------------------------
		const FMatrix LightRotationView = FMatrix::LookAtLH(
			FVector(0.0f, 0.0f, 0.0f),
			LightDir,
			Up);

		const FVector LightSpaceCenter = LightRotationView.TransformPositionWithW(Center);
		const float SnappedCenterX = SnapToTexelGrid(LightSpaceCenter.X, TexelSize);
		const float SnappedCenterY = SnapToTexelGrid(LightSpaceCenter.Y, TexelSize);

		// 5. light direction 기준 receiver depth 범위를 계산하기 위한 초기값.
		FLightSpaceBounds ReceiverBounds;

		// ------------------------------------------------------------
		// 6. 월드 공간의 cascade 코너 8개를 light space로 변환한다.
		//
		// 변환된 좌표 LS의 의미:
		// - LS.X : 빛 기준 오른쪽/왼쪽 방향 위치
		// - LS.Y : 빛 기준 위/아래 방향 위치
		// - LS.Z : 빛이 바라보는 방향으로의 깊이
		//
		// X/Y는 bounding sphere와 texel snapping으로 처리하므로,
		// 여기서는 receiver depth를 안정적으로 포함하기 위한 Z 범위만 사용한다.
		// ------------------------------------------------------------
		for (int i = 0; i < 8; ++i)
		{
			FVector LS = LightRotationView.TransformPositionWithW(WorldCorners[i]);
			IncludeLightSpacePoint(ReceiverBounds, LS);
		}

		// ------------------------------------------------------------
		// 7. receiver와 같은 XY 투영 영역에 있는 caster bounds를 depth 범위에 포함한다.
		//
		// Directional shadow에서는 light-space XY가 같아야 receiver 위로 투영된다.
		// 따라서 전체 씬 bounds를 무작정 포함하지 않고, 현재 cascade ortho 영역과 XY가 겹치는
		// shadow caster만 Z 범위에 추가해서 불필요한 depth precision 손실을 줄인다.
		// ------------------------------------------------------------
		const float HalfOrthoSize = OrthoSize * 0.5f;
		const float ProjectionMinX = SnappedCenterX - HalfOrthoSize;
		const float ProjectionMaxX = SnappedCenterX + HalfOrthoSize;
		const float ProjectionMinY = SnappedCenterY - HalfOrthoSize;
		const float ProjectionMaxY = SnappedCenterY + HalfOrthoSize;
		const float XYOverlapPadding = (std::max)(TexelSize * 2.0f, 1.0f);

		float FinalMinZ = ReceiverBounds.MinZ;
		float FinalMaxZ = ReceiverBounds.MaxZ;

		for (uint32 BoundsIndex = 0; BoundsIndex < CasterBoundsCount; ++BoundsIndex)
		{
			const FBoundingBox& WorldBounds = CasterBounds[BoundsIndex];
			if (!WorldBounds.IsValid())
			{
				continue;
			}

			++Result.DebugCandidateCasterCount;

			const FLightSpaceBounds CasterLSBounds = BuildLightSpaceBounds(LightRotationView, WorldBounds);
			if (!OverlapsLightSpaceXY(
				CasterLSBounds,
				ProjectionMinX,
				ProjectionMinY,
				ProjectionMaxX,
				ProjectionMaxY,
				XYOverlapPadding))
			{
				continue;
			}

			FinalMinZ = (std::min)(FinalMinZ, CasterLSBounds.MinZ);
			FinalMaxZ = (std::max)(FinalMaxZ, CasterLSBounds.MaxZ);
			++Result.DebugIncludedCasterCount;
		}

		// ------------------------------------------------------------
		// 8. 최종 light depth 범위를 계산한다.
		//
		// 기존 구현은 receiver 중심을 기준으로 depth range를 대칭 배치했다.
		// 그러면 caster가 receiver보다 빛 쪽으로 멀리 떨어진 경우, 카메라 회전만으로
		// caster 전체가 shadow camera near/far 밖으로 밀려 CSM slice가 비어버릴 수 있다.
		// 여기서는 실제 caster bounds를 포함한 뒤, 추가로 caster distance만큼 빛 쪽 여유를 둔다.
		// ------------------------------------------------------------
		const float ReceiverMinZ = ReceiverBounds.MinZ;
		const float ReceiverMaxZ = ReceiverBounds.MaxZ;
		const float CasterDistance = (std::max)(0.0f, FShadowSettings::Get().GetEffectiveCSMCasterDistance());

		FinalMinZ = (std::min)(FinalMinZ, ReceiverMinZ - CasterDistance);
		FinalMaxZ = (std::max)(FinalMaxZ, ReceiverMaxZ + CSMReceiverDepthPadding);

		const float PaddedMinZ = FinalMinZ - CSMReceiverDepthPadding;
		const float PaddedMaxZ = FinalMaxZ + CSMReceiverDepthPadding;
		const float PaddedDepthRange = (std::max)(PaddedMaxZ - PaddedMinZ, 1.0f);

		Result.DebugReceiverMinZ = ReceiverMinZ;
		Result.DebugReceiverMaxZ = ReceiverMaxZ;
		Result.DebugFinalMinZ = PaddedMinZ;
		Result.DebugFinalMaxZ = PaddedMaxZ;

		// ------------------------------------------------------------
		// 9. light space에서 최종 shadow camera 위치를 정한다.
		//
		// X/Y는 shadow texel grid에 스냅된 중심을 사용한다.
		// Z는 caster/receiver를 모두 포함한 최소 Z에서 시작시켜
		// NearZ=0, FarZ=PaddedDepthRange 형태의 ortho projection과 맞춘다.
		// ------------------------------------------------------------
		const FMatrix InvLightRotationView = LightRotationView.GetInverseFast();
		const FVector LightSpaceEye(SnappedCenterX, SnappedCenterY, PaddedMinZ);
		const FVector WorldEye = InvLightRotationView.TransformPositionWithW(LightSpaceEye);

		// ------------------------------------------------------------
		// 10. 최종 light view matrix를 만든다.
		//
		// Target = WorldEye + LightDir 이므로,
		// shadow camera는 directional light 방향과 같은 +Z 방향을 바라본다.
		//
		// 이 view matrix가 실제 shadow map을 렌더링할 때 사용되는 light view다.
		// ------------------------------------------------------------
		Result.View = FMatrix::LookAtLH(
			WorldEye,
			WorldEye + LightDir,
			Up
		);

		// ------------------------------------------------------------
		// 11. orthographic projection 파라미터를 저장한다.
		//
		// light camera의 위치 자체를 PaddedMinZ에 맞췄기 때문에,
		// NearZ는 0부터 시작하고 FarZ는 PaddedDepthRange가 된다.
		// ------------------------------------------------------------
		Result.OrthoWidth = OrthoSize;
		Result.OrthoHeight = OrthoSize;
		Result.NearZ = 0.0f;
		Result.FarZ = PaddedDepthRange;

		// 12. 최종 orthographic projection matrix를 만든다.
		Result.Proj = FMatrix::OrthoLH(
			Result.OrthoWidth,
			Result.OrthoHeight,
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
