#include "Camera/FCamera.h"
#include <cmath>

FMatrix FCamera::GetViewMatrix() const 
{
	FVector Forward;
	Forward.X = cosf(Pitch) * sinf(Yaw);
	Forward.Y = sinf(Pitch);
	Forward.Z = cosf(Pitch) * cosf(Yaw);

	Forward.Normalize();

	// @@ 여기서 사용하는 UpVector는 임의의 카메라 UpVector
	FVector Right = FVector::UpVector.Cross(Forward).GetNormalized();
	FVector Up = Forward.Cross(Right); 

	FMatrix View = FMatrix::MakeIdentity();

	View.M[0][0] = Right.X;
	View.M[1][0] = Right.Y;
	View.M[2][0] = Right.Z;

	View.M[0][1] = Up.X;
	View.M[1][1] = Up.Y;
	View.M[2][1] = Up.Z;

	View.M[0][2] = Forward.X;
	View.M[1][2] = Forward.Y;
	View.M[2][2] = Forward.Z;

	return View;
}

FMatrix FCamera::GetProjectionMatrix() const
{
	const float TanHalfFOV = tanf(FOV * 0.5f * 3.1415926535f / 180.0f);

	FMatrix Result;

	Result.M[0][0] = 1.0f / (Aspect * TanHalfFOV);
	Result.M[1][1] = 1.0f / TanHalfFOV;
	Result.M[2][2] = FarZ / (FarZ - NearZ);
	Result.M[2][3] = 1.0f;
	Result.M[3][2] = (-NearZ * FarZ) / (FarZ - NearZ);

	return Result;
}