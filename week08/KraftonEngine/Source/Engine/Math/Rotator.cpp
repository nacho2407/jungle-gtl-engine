#include "Rotator.h"
#include "Quat.h"
#include "Matrix.h"

const FRotator FRotator::ZeroRotator = FRotator(0.0f, 0.0f, 0.0f);

FQuat FRotator::ToQuaternion() const
{
	return FQuat::FromRotator(*this);
}

FMatrix FRotator::ToMatrix() const
{
	return FMatrix::MakeRotationEuler(ToVector());
}

FVector FRotator::GetForwardVector() const
{
	float PitchRad = Pitch * DEG_TO_RAD;
	float YawRad = Yaw * DEG_TO_RAD;
	float CP = cosf(PitchRad);
	return FVector(CP * cosf(YawRad), CP * sinf(YawRad), -sinf(PitchRad));
}

FVector FRotator::GetRightVector() const
{
	return ToQuaternion().GetRightVector();
}

FVector FRotator::GetUpVector() const
{
	return ToQuaternion().GetUpVector();
}

FRotator FRotator::FromQuaternion(const FQuat& Quat)
{
	return Quat.ToRotator();
}
