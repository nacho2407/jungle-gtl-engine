#pragma once

#include "EngineAPI.h"
#include "Math/Vector.h"

struct FQuat;

struct ENGINE_API FRotator
{
	float Pitch = 0.0f;
	float Yaw = 0.0f;
	float Roll = 0.0f;

	static const FRotator ZeroRotator;

	constexpr FRotator() noexcept = default;

	constexpr FRotator(float InPitch, float InYaw, float InRoll) noexcept
		: Pitch(InPitch), Yaw(InYaw), Roll(InRoll)
	{
	}

	explicit FRotator(const FQuat& InQuat) noexcept;

	// 각도를 [0, 360) 범위로 감쌉니다.
	static float ClampAxis(float AngleDegrees) noexcept;
	// 각도를 (-180, 180] 범위로 정규화합니다.
	static float NormalizeAxis(float AngleDegrees) noexcept;
	// 엔진 오일러 각도 {Roll, Pitch, Yaw}로부터 로테이터를 생성합니다.
	static FRotator MakeFromEuler(const FVector& InEulerDegrees) noexcept;

	bool operator==(const FRotator& Other) const noexcept;
	bool operator!=(const FRotator& Other) const noexcept;
	FRotator operator-() const noexcept;
	FRotator operator+(const FRotator& Other) const noexcept;
	FRotator operator-(const FRotator& Other) const noexcept;
	FRotator operator*(float Scale) const noexcept;
	FRotator operator/(float Scale) const noexcept;

	FRotator& operator+=(const FRotator& Other) noexcept;
	FRotator& operator-=(const FRotator& Other) noexcept;
	FRotator& operator*=(float Scale) noexcept;
	FRotator& operator/=(float Scale) noexcept;

	// 이 로테이터를 엔진 오일러 각도 {Roll, Pitch, Yaw}로 반환합니다.
	FVector Euler() const noexcept;
	// 이 로테이터가 바라보는 전방 벡터(+X)를 반환합니다.
	FVector Vector() const noexcept;
	// 이 로테이터 회전을 벡터에 적용합니다.
	FVector RotateVector(const FVector& InVector) const noexcept;
	// 이 로테이터의 역회전을 벡터에 적용합니다.
	FVector UnrotateVector(const FVector& InVector) const noexcept;
	// Pitch, Yaw, Roll에 델타 값을 더하고 결과를 반환합니다.
	FRotator Add(float DeltaPitch, float DeltaYaw, float DeltaRoll) noexcept;
	// 성분 중 하나라도 NaN 또는 무한대이면 true를 반환합니다.
	bool ContainsNaN() const noexcept;
	// 정규화 기준으로 모든 각도가 정확히 0이면 true를 반환합니다.
	bool IsZero() const noexcept;
	// 허용 오차 내에서 다른 로테이터와 같은 회전인지 비교합니다.
	bool Equals(const FRotator& Other, float Tolerance = 1.e-6f) const noexcept;
	// 허용 오차 내에서 항등 회전에 가까운지 반환합니다.
	bool IsNearlyZero(float Tolerance = 1.e-6f) const noexcept;
	// 다른 로테이터와의 축별 각도 차이 절댓값 합을 반환합니다.
	float GetManhattanDistance(const FRotator& Other) const noexcept;
	// 전달된 로테이터를 현재 로테이터에 가장 가까운 각도 표현으로 맞춥니다.
	void SetClosestToMe(FRotator& MakeClosest) const noexcept;
	// 각 성분을 [0, 360) 범위로 보정합니다.
	void Clamp() noexcept;
	// 각 성분을 (-180, 180] 범위로 정규화합니다.
	void Normalize() noexcept;
	// 각 성분을 [0, 360) 범위로 보정한 복사본을 반환합니다.
	FRotator GetDenormalized() const noexcept;
	// 각 성분을 (-180, 180] 범위로 정규화한 복사본을 반환합니다.
	FRotator GetNormalized() const noexcept;
	// 이 로테이터의 역회전을 나타내는 로테이터를 반환합니다.
	FRotator GetInverse() const noexcept;

	// 이 로테이터를 대응하는 쿼터니언으로 변환합니다.
	FQuat Quaternion() const noexcept;
};

inline FRotator operator*(float Scale, const FRotator& Rotator) noexcept
{
	return Rotator * Scale;
}
