#pragma once
#include "Math/FVector.h"
#include "Math/FMatrix.h"

struct FCamera
{
	FVector Position = { 0.0f, 0.0f, -0.5f };

	float Yaw = 0.0f;
	float Pitch = 0.0f;

	float FOV = 60.0f;
	float Aspect = 16.0f / 9.0f;

	float NearZ = 0.1f;
	float FarZ = 1000.0f;

public:
	FMatrix GetViewMatrix() const;
	FMatrix GetProjectionMatrix() const;
};