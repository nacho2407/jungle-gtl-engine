#include "Render/Types/MinimalViewInfo.h"

#include <cmath>

FMatrix FMinimalViewInfo::CalculateViewMatrix() const
{
	const FVector Forward = Rotation.GetForwardVector();
	const FVector Right   = Rotation.GetRightVector();
	const FVector Up      = Rotation.GetUpVector();
	return FMatrix::MakeViewMatrix(Right, Up, Forward, Location);
}

FMatrix FMinimalViewInfo::CalculateProjectionMatrix() const
{
	if (bIsOrtho)
	{
		const float HalfW = OrthoWidth * 0.5f;
		const float HalfH = HalfW / AspectRatio;
		return FMatrix::OrthoLH(HalfW * 2.0f, HalfH * 2.0f, NearClip, FarClip);
	}
	return FMatrix::PerspectiveFovLH(FOV, AspectRatio, NearClip, FarClip);
}

FMatrix FMinimalViewInfo::CalculateViewProjectionMatrix() const
{
	return CalculateViewMatrix() * CalculateProjectionMatrix();
}

FRay FMinimalViewInfo::DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight) const
{
	const float NdcX = (2.0f * MouseX) / ScreenWidth - 1.0f;
	const float NdcY = 1.0f - (2.0f * MouseY) / ScreenHeight;

	// Reversed-Z 컨벤션: near plane = 1, far plane = 0 (codebase 전체 일관)
	const FVector NdcNear(NdcX, NdcY, 1.0f);
	const FVector NdcFar (NdcX, NdcY, 0.0f);

	const FMatrix InvVP = CalculateViewProjectionMatrix().GetInverse();

	const FVector WorldNear = InvVP.TransformPositionWithW(NdcNear);
	const FVector WorldFar  = InvVP.TransformPositionWithW(NdcFar);

	FRay Ray;
	Ray.Origin = WorldNear;

	const FVector Dir = WorldFar - WorldNear;
	const float Length = std::sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y + Dir.Z * Dir.Z);
	Ray.Direction = (Length > 1e-4f) ? Dir / Length : FVector(1.0f, 0.0f, 0.0f);

	return Ray;
}
