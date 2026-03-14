#pragma once

#include <algorithm>

#include "FMatrix.h"
#include "FVector.h"
#include "UObject.h"
#include "InputManager.h"

const FVector UpVec(0.0f, 1.0f, 0.0f);
const float Epsilon = 1e-6f;

class UCamera : public UObject {
	DECLARE_RTTI(UCamera, UObject)
public:
	FVector Position{ FVector(0.0f, 0.0f, 0.0f) };
	float Pitch{ 0.0f };
	float Yaw{ 0.0f };
	float Roll{ 0.0f };
	bool IsChanged{ true };
	float MoveSpeed{ 0.1f };

	float FOV{ 60.0f };
	float AspectRatio{ 1.0f };
	float NearPlane{ 0.1f };
	float FarPlane{ 1000.0f };

	FVector Direction{ FVector(0.0f, 0.0f, 1.0f) };
	FVector SideDirection{ FVector(1.0f, 0.0f, 0.0f) };
	FVector UpDirection{ FVector(0.0f, 1.0f, 0.0f) };

	FMatrix View;
	FMatrix Projection;
	FMatrix ViewProjection;

public:
	UCamera() {}
	virtual ~UCamera() override;

	void SetPosition(const FVector& _input);
	void Move();
	void SetDirection(const float InputPitch, const float InputYaw,
		const float InputRoll);
	void Rotate();
	FMatrix GetViewProjectionMatrix() const;

	void OnResize(int NewWidth, int NewHeight);
	void update();
	FMatrix GetViewMatrix() const;
	FMatrix GetProjectionMatrix() const;

private:
};
