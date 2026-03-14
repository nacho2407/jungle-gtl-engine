#pragma once

#include <algorithm>

#include "FMatrix.h"
#include "FVector.h"
#include "UObject.h"
#include "InputManager.h"

enum class EProjectionType
{
	Perspective,
	Orthogonal,
	Isometric
};

const FVector UpVec(0.0f, 1.0f, 0.0f);
const float Epsilon = 1e-6f;

class UCamera : public UObject {
	DECLARE_RTTI(UCamera, UObject)
public:
	FVector Position{ FVector(0.0f, 0.0f, 0.0f) };
	float Pitch{ 0.0f };
	float Yaw{ 0.0f };
	float Roll{ 0.0f };
	float FOV{ 60.0f };
	float AspectRatio{ 1.0f };
	float NearPlane{ 1.0f };
	float FarPlane{ 2000.0f };
	bool IsChanged{ true };
	float MoveSpeed{ 0.1f };
	float ScreenWidth = 0.f;
	float ScreenHeight = 0.f;
	float RotateSensitivity = 0.1f;
	bool IsOrthogonal{ false };
	float OrthoHeight{ 10.0f };

	//EProjectionType ProjectionType{ EProjectionType::Perspective };

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
	void SetFOV(const float NewFOV);
	float GetFOV() const;
	void OnResize(int NewWidth, int NewHeight);
	float GetAspectRatio() const;
	void SetOrthogonal(bool input);
	//void ChangePerpective(EProjectionType NewType);
	void update();
	void CalcViewMatrix();
	FMatrix GetViewMatrix() const;
	void CalcProjectionMatrix();
	FMatrix GetProjectionMatrix() const;
	void CalcPerspectiveProjectionMatrix();
	void CalcOrthogonalProjectionMatrix();
	FMatrix GetViewProjectionMatrix() const;

private:

};
