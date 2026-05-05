#include "Editor/Viewport/ViewportCameraTransform.h"

#include "Math/MathUtils.h"

#include <cmath>

void FViewportCameraTransform::TranslateLocal(const FVector& LocalDelta)
{
	const FVector Forward = ViewRotation.GetForwardVector();
	const FVector Right   = ViewRotation.GetRightVector();
	const FVector Up      = ViewRotation.GetUpVector();
	ViewLocation += Forward * LocalDelta.X
	              + Right   * LocalDelta.Y
	              + Up      * LocalDelta.Z;
}

void FViewportCameraTransform::Rotate(float DeltaYaw, float DeltaPitch)
{
	ViewRotation.Yaw   += DeltaYaw;
	ViewRotation.Pitch += DeltaPitch;
	ViewRotation.Pitch = Clamp(ViewRotation.Pitch, -89.9f, 89.9f);
	ViewRotation.Roll  = 0.0f;
}

void FViewportCameraTransform::LookAt(const FVector& Target)
{
	const FVector Diff = (Target - ViewLocation).Normalized();
	constexpr float Rad2Deg = 180.0f / 3.14159265358979f;

	ViewRotation.Pitch = -asinf(Diff.Z) * Rad2Deg;
	if (fabsf(Diff.Z) < 0.999f)
	{
		ViewRotation.Yaw = atan2f(Diff.Y, Diff.X) * Rad2Deg;
	}
}
