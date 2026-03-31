#pragma once
#include <cmath>
#include "Vector.h"
#include "Utils.h"

struct FQuat;
struct FMatrix;

// 오일러 각도 회전 (도 단위). UE 컨벤션: Pitch=Y축, Yaw=Z축, Roll=X축
struct FRotator
{
	float Pitch;	// Y축 회전 (도)
	float Yaw;		// Z축 회전 (도)
	float Roll;		// X축 회전 (도)

	FRotator() : Pitch(0.0f), Yaw(0.0f), Roll(0.0f) {}
	FRotator(float InPitch, float InYaw, float InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}

	// FVector 호환: FVector(X=Roll, Y=Pitch, Z=Yaw) ↔ FRotator
	explicit FRotator(const FVector& Euler) : Pitch(Euler.Y), Yaw(Euler.Z), Roll(Euler.X) {}
	FVector ToVector() const { return FVector(Roll, Pitch, Yaw); }

	FRotator operator+(const FRotator& Other) const
	{
		return FRotator(Pitch + Other.Pitch, Yaw + Other.Yaw, Roll + Other.Roll);
	}

	FRotator operator-(const FRotator& Other) const
	{
		return FRotator(Pitch - Other.Pitch, Yaw - Other.Yaw, Roll - Other.Roll);
	}

	FRotator operator*(float Scale) const
	{
		return FRotator(Pitch * Scale, Yaw * Scale, Roll * Scale);
	}

	FRotator& operator+=(const FRotator& Other)
	{
		Pitch += Other.Pitch; Yaw += Other.Yaw; Roll += Other.Roll;
		return *this;
	}

	FRotator& operator-=(const FRotator& Other)
	{
		Pitch -= Other.Pitch; Yaw -= Other.Yaw; Roll -= Other.Roll;
		return *this;
	}

	bool operator==(const FRotator& Other) const
	{
		return fabsf(Pitch - Other.Pitch) < EPSILON
			&& fabsf(Yaw - Other.Yaw) < EPSILON
			&& fabsf(Roll - Other.Roll) < EPSILON;
	}

	bool operator!=(const FRotator& Other) const { return !(*this == Other); }

	// 각도를 [0, 360) 범위로 정규화
	FRotator GetNormalized() const
	{
		return FRotator(NormalizeAxis(Pitch), NormalizeAxis(Yaw), NormalizeAxis(Roll));
	}

	// 각도를 (-180, 180] 범위로 클램프
	FRotator GetClamped() const
	{
		return FRotator(ClampAxis(Pitch), ClampAxis(Yaw), ClampAxis(Roll));
	}

	bool IsNearlyZero(float Tolerance = EPSILON) const
	{
		return fabsf(Pitch) < Tolerance && fabsf(Yaw) < Tolerance && fabsf(Roll) < Tolerance;
	}

	static const FRotator ZeroRotator;

	// 변환 — 선언만, 구현은 Rotator.cpp (순환 의존 방지)
	FQuat ToQuaternion() const;
	FMatrix ToMatrix() const;
	FVector GetForwardVector() const;
	FVector GetRightVector() const;
	FVector GetUpVector() const;

	static FRotator FromQuaternion(const FQuat& Quat);

private:
	static float NormalizeAxis(float Angle)
	{
		Angle = fmodf(Angle, 360.0f);
		if (Angle < 0.0f) Angle += 360.0f;
		return Angle;
	}

	static float ClampAxis(float Angle)
	{
		Angle = fmodf(Angle + 180.0f, 360.0f);
		if (Angle < 0.0f) Angle += 360.0f;
		return Angle - 180.0f;
	}
};
