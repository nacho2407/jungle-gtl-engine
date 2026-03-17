#pragma once

#include "Object.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

enum class EProjectionType
{
	Perspective,
	Orthogonal,
	Isometric
};

const FVector UpVec(0.0f, 0.0f, 1.0f);
const float Epsilon = 1e-6f;

class UCamera : public UObject {
	DECLARE_RTTI(UCamera, UObject)
public:
	FVector Position{ FVector(0.0f, 0.0f, 0.0f) };
	float Pitch{ 0.0f };
	float Yaw{ 0.0f };
	float Roll{ 0.0f };
	float FOV{ 90.0f };
	float AspectRatio{ 16.0f / 9.0f };
	float NearPlane{ 1.0f };
	float FarPlane{ 2000.0f };
	bool bIsDirty{ true };
	float MoveSpeed{ 10.0f };
	float ScreenWidth = 0.f;
	float ScreenHeight = 0.f;
	float RotateSensitivity = 500.0f;
	bool IsOrthogonal{ false };
	float OrthoHeight{ 10.0f };

	float FocusLength{ 5.0f };
	FVector FocusPoint;

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
	void Move(float DeltaTime);
	void SetDirection(const float InputPitch, const float InputYaw,
		const float InputRoll);
	void Rotate(float DeltaTime);
	void OrbitRotate(float DeltaTime);
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
