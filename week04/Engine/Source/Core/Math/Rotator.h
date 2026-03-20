#pragma once

#include "../EngineAPI.h"

struct FMatrix;

struct ENGINE_API FRotator
{
public:
	FRotator();
	FRotator(float InRoll, float InPitch, float InYaw);

	FMatrix ToMatrix() const;
	void Normalize();

	FRotator operator+(const FRotator& Other) const;
	FRotator operator-(const FRotator& Other) const;
	FRotator& operator+=(const FRotator& Other);
	FRotator& operator-=(const FRotator& Other);

	static FRotator FromVector(const FVector& Forward);
	static FRotator ZeroRotator();

public:
	float Roll;  // X축 회전
	float Pitch; // Y축 회전
	float Yaw;  // Z축 회전
};
