#pragma once

#include "Core/CoreMinimal.h"
#include "Particle/ParticleTypes.h"

#include <algorithm>
#include <cmath>

namespace ParticleBeamPath
{
	/**
	 * @brief Beam replay point를 world space point로 변환합니다.
	 *
	 * @param ReplayData 좌표계 정보를 가진 Beam replay data
	 *
	 * @param ComponentToWorld local point를 world point로 변환할 component world matrix
	 *
	 * @param Point 변환할 replay point
	 *
	 * @return world space point
	 */
	inline FVector GetBeamWorldPoint(
		const FDynamicBeamEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FVector& Point)
	{
		// replay coordinate space 기준 world 변환
		return ReplayData.CoordinateSpace == EParticleCoordinateSpace::Local
			? ComponentToWorld.TransformPosition(Point)
			: Point;
	}

	/**
	 * @brief Beam replay tangent를 world space vector로 변환합니다.
	 *
	 * @param ReplayData 좌표계 정보를 가진 Beam replay data
	 *
	 * @param ComponentToWorld local tangent를 world vector로 변환할 component world matrix
	 *
	 * @param Tangent 변환할 replay tangent
	 *
	 * @return world space tangent vector
	 */
	inline FVector GetBeamWorldTangent(
		const FDynamicBeamEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		const FVector& Tangent)
	{
		// tangent는 방향/크기 vector이므로 translation 없는 vector transform 사용
		return ReplayData.CoordinateSpace == EParticleCoordinateSpace::Local
			? ComponentToWorld.TransformVector(Tangent)
			: Tangent;
	}

	/**
	 * @brief Beam segment 수를 계산합니다.
	 *
	 * @param ReplayData interpolation과 noise 설정을 가진 Beam replay data
	 *
	 * @return 생성할 Beam segment 수
	 */
	inline int32 CalculateBeamSegmentCount(const FDynamicBeamEmitterReplayDataBase& ReplayData)
	{
		// interpolation 기준 segment 수
		const int32 InterpolationSegmentCount = std::max(
			std::clamp(ReplayData.InterpolationPoints, 0, 64) + 1,
			1);

		// noise 기준 최소 segment 수
		if (!ReplayData.bNoiseEnabled ||
			ReplayData.NoiseRange <= 0.0f ||
			ReplayData.NoiseFrequency <= 0)
		{
			return InterpolationSegmentCount;
		}

		const int32 NoiseSegmentCount = std::max(
			std::clamp(ReplayData.NoiseFrequency, 0, 64) + 1,
			1);
		return std::max(InterpolationSegmentCount, NoiseSegmentCount);
	}

	/**
	 * @brief Cubic Hermite curve point를 평가합니다.
	 *
	 * @param Source source point
	 *
	 * @param SourceTangent source tangent vector
	 *
	 * @param Target target point
	 *
	 * @param TargetTangent target tangent vector
	 *
	 * @param Alpha source에서 target까지의 보간 비율
	 *
	 * @return Cubic Hermite curve 위의 point
	 */
	inline FVector EvaluateCubicHermite(
		const FVector& Source,
		const FVector& SourceTangent,
		const FVector& Target,
		const FVector& TargetTangent,
		float Alpha)
	{
		// Hermite basis 계산
		const float T = std::clamp(Alpha, 0.0f, 1.0f);
		const float T2 = T * T;
		const float T3 = T2 * T;
		const float H00 = 2.0f * T3 - 3.0f * T2 + 1.0f;
		const float H10 = T3 - 2.0f * T2 + T;
		const float H01 = -2.0f * T3 + 3.0f * T2;
		const float H11 = T3 - T2;

		return
			Source * H00 +
			SourceTangent * H10 +
			Target * H01 +
			TargetTangent * H11;
	}

	/**
	 * @brief Beam noise offset용 수직 평면 basis를 계산합니다.
	 *
	 * @param Source Beam source world point
	 *
	 * @param Target Beam target world point
	 *
	 * @param OutBasisA 첫 번째 noise basis vector
	 *
	 * @param OutBasisB 두 번째 noise basis vector
	 */
	inline void BuildBeamNoiseBasis(
		const FVector& Source,
		const FVector& Target,
		FVector& OutBasisA,
		FVector& OutBasisB)
	{
		// Beam 방향 기준 직교 basis 계산
		FVector BeamDirection = (Target - Source).GetSafeNormal();
		if (BeamDirection.IsNearlyZero())
		{
			BeamDirection = FVector::ForwardVector;
		}

		BeamDirection.FindBestAxisVectors(OutBasisA, OutBasisB);
		if (OutBasisA.IsNearlyZero())
		{
			OutBasisA = FVector::RightVector;
		}
		if (OutBasisB.IsNearlyZero())
		{
			OutBasisB = FVector::UpVector;
		}
	}

	/**
	 * @brief deterministic noise hash 값을 생성합니다.
	 */
	inline uint32 HashBeamNoise(uint32 Value)
	{
		// 작고 빠른 avalanche hash
		Value ^= Value >> 16;
		Value *= 0x7feb352du;
		Value ^= Value >> 15;
		Value *= 0x846ca68bu;
		Value ^= Value >> 16;
		return Value;
	}

	/**
	 * @brief deterministic noise scalar를 생성합니다.
	 */
	inline float MakeBeamNoiseScalar(
		int32 Seed,
		int32 PointIndex,
		int32 TimeSlice,
		uint32 Channel)
	{
		// seed / point / time / channel 조합
		uint32 Hash = static_cast<uint32>(Seed);
		Hash ^= static_cast<uint32>(PointIndex) * 0x9e3779b9u;
		Hash ^= static_cast<uint32>(TimeSlice) * 0x85ebca6bu;
		Hash ^= Channel * 0xc2b2ae35u;
		Hash = HashBeamNoise(Hash);

		// [0, 1]을 [-1, 1]로 변환
		const float Unit = static_cast<float>(Hash >> 8) * (1.0f / 16777216.0f);
		return Unit * 2.0f - 1.0f;
	}

	/**
	 * @brief smoothstep interpolation alpha를 계산합니다.
	 */
	inline float SmoothStep01(float Alpha)
	{
		// time slice 사이의 부드러운 보간 alpha
		const float T = std::clamp(Alpha, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/**
	 * @brief Beam noise offset을 계산합니다.
	 *
	 * @param ReplayData noise 설정을 가진 Beam replay data
	 *
	 * @param PointIndex noise를 적용할 path point index
	 *
	 * @param BasisA 첫 번째 noise basis vector
	 *
	 * @param BasisB 두 번째 noise basis vector
	 *
	 * @return world space noise offset
	 */
	inline FVector CalculateBeamNoiseOffset(
		const FDynamicBeamEmitterReplayDataBase& ReplayData,
		int32 PointIndex,
		const FVector& BasisA,
		const FVector& BasisB)
	{
		// 정적 noise와 animated noise 공통 time 위치
		const float SafeNoiseSpeed = std::max(ReplayData.NoiseSpeed, 0.0f);
		const float TimePosition = SafeNoiseSpeed > 0.0f
			? ReplayData.BeamTimeSeconds * SafeNoiseSpeed
			: 0.0f;
		const int32 TimeSlice0 = static_cast<int32>(std::floor(TimePosition));
		const int32 TimeSlice1 = TimeSlice0 + 1;
		const float TimeAlpha = SmoothStep01(TimePosition - static_cast<float>(TimeSlice0));

		// 시간 slice별 deterministic 2D noise
		const float NoiseAX0 = MakeBeamNoiseScalar(ReplayData.NoiseSeed, PointIndex, TimeSlice0, 0u);
		const float NoiseAY0 = MakeBeamNoiseScalar(ReplayData.NoiseSeed, PointIndex, TimeSlice0, 1u);
		const float NoiseAX1 = MakeBeamNoiseScalar(ReplayData.NoiseSeed, PointIndex, TimeSlice1, 0u);
		const float NoiseAY1 = MakeBeamNoiseScalar(ReplayData.NoiseSeed, PointIndex, TimeSlice1, 1u);

		// slice 사이 보간 noise
		const float NoiseA = NoiseAX0 + (NoiseAX1 - NoiseAX0) * TimeAlpha;
		const float NoiseB = NoiseAY0 + (NoiseAY1 - NoiseAY0) * TimeAlpha;
		const float SafeNoiseRange = std::max(ReplayData.NoiseRange, 0.0f);
		return BasisA * (NoiseA * SafeNoiseRange) + BasisB * (NoiseB * SafeNoiseRange);
	}

	/**
	 * @brief Beam 중심선 path point를 생성합니다.
	 *
	 * @param ReplayData source / target / tangent / interpolation / noise 설정을 가진 Beam replay data
	 *
	 * @param ComponentToWorld local replay 값을 world로 변환할 component world matrix
	 *
	 * @param OutPoints 생성된 world space path point 목록
	 */
	inline void BuildBeamPathPoints(
		const FDynamicBeamEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		TArray<FVector>& OutPoints)
	{
		OutPoints.clear();

		// replay coordinate space 기준 source / target / tangent를 world space로 변환
		const FVector Source = GetBeamWorldPoint(ReplayData, ComponentToWorld, ReplayData.SourcePoint);
		const FVector Target = GetBeamWorldPoint(ReplayData, ComponentToWorld, ReplayData.TargetPoint);
		const FVector SourceTangent = GetBeamWorldTangent(ReplayData, ComponentToWorld, ReplayData.SourceTangent);
		const FVector TargetTangent = GetBeamWorldTangent(ReplayData, ComponentToWorld, ReplayData.TargetTangent);

		// segment N개를 만들기 위한 point N+1개
		const int32 SegmentCount = CalculateBeamSegmentCount(ReplayData);
		OutPoints.reserve(static_cast<size_t>(SegmentCount + 1));

		// InterpolationPoints 비활성 시 직선 중심선 의미 보존
		const bool bUseHermitePath = std::clamp(ReplayData.InterpolationPoints, 0, 64) > 0;
		const FVector BeamDelta = Target - Source;

		for (int32 PointIndex = 0; PointIndex <= SegmentCount; ++PointIndex)
		{
			// segment point 정규화 위치
			const float Alpha = static_cast<float>(PointIndex) / static_cast<float>(SegmentCount);

			// interpolation이 켜진 경우에만 tangent 기반 Hermite path 사용
			const FVector PathPoint = bUseHermitePath
				? EvaluateCubicHermite(Source, SourceTangent, Target, TargetTangent, Alpha)
				: Source + BeamDelta * Alpha;
			OutPoints.push_back(PathPoint);
		}

		// noise 비활성 또는 유효 범위 없음
		if (!ReplayData.bNoiseEnabled ||
			ReplayData.NoiseRange <= 0.0f ||
			ReplayData.NoiseFrequency <= 0 ||
			OutPoints.size() <= 2)
		{
			return;
		}

		// Beam 방향 수직 평면 noise basis
		FVector BasisA;
		FVector BasisB;
		BuildBeamNoiseBasis(Source, Target, BasisA, BasisB);

		// endpoint 제외 중간 point noise 적용
		for (int32 PointIndex = 1; PointIndex + 1 < static_cast<int32>(OutPoints.size()); ++PointIndex)
		{
			OutPoints[static_cast<size_t>(PointIndex)] +=
				CalculateBeamNoiseOffset(ReplayData, PointIndex, BasisA, BasisB);
		}
	}

	/**
	 * @brief Beam path point와 half width에서 world bounds를 생성합니다.
	 *
	 * @param BeamPoints world space Beam path point 목록
	 *
	 * @param HalfWidth Beam segment half width
	 *
	 * @return Beam path 전체를 포함하는 world bounds
	 */
	inline FBoundingBox BuildBeamPointWorldBounds(
		const TArray<FVector>& BeamPoints,
		float HalfWidth)
	{
		// segment별 quad가 차지할 수 있는 보수적 두께 extent
		const float SafeHalfWidth = std::max(HalfWidth, 0.05f);
		const FVector Extent(SafeHalfWidth, SafeHalfWidth, SafeHalfWidth);

		FBoundingBox Bounds;
		for (const FVector& Point : BeamPoints)
		{
			Bounds.Expand(Point - Extent);
			Bounds.Expand(Point + Extent);
		}
		return Bounds;
	}

	/**
	 * @brief Beam replay data와 half width에서 world bounds를 생성합니다.
	 *
	 * @param ReplayData source / target / tangent / interpolation / noise 설정을 가진 Beam replay data
	 *
	 * @param ComponentToWorld local replay 값을 world로 변환할 component world matrix
	 *
	 * @param HalfWidth Beam segment half width
	 *
	 * @return Beam path 전체를 포함하는 world bounds
	 */
	inline FBoundingBox BuildBeamWorldBounds(
		const FDynamicBeamEmitterReplayDataBase& ReplayData,
		const FMatrix& ComponentToWorld,
		float HalfWidth)
	{
		// render와 culling이 공유하는 최종 path point 기준 bounds
		TArray<FVector> BeamPoints;
		BuildBeamPathPoints(ReplayData, ComponentToWorld, BeamPoints);
		return BuildBeamPointWorldBounds(BeamPoints, HalfWidth);
	}
}
