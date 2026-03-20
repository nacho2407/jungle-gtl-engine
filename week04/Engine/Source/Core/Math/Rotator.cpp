#include "../CoreMinimal.h"

FRotator::FRotator() : Roll(0.0f), Pitch(0.0f), Yaw(0.0f) {

}

FRotator::FRotator(float InRoll, float InPitch, float InYaw)
	: Roll(InRoll), Pitch(InPitch), Yaw(InYaw)
{

}

FMatrix FRotator::ToMatrix() const
{
	return FMatrix::RotationXMatrix(Roll) * FMatrix::RotationYMatrix(Pitch) * FMatrix::RotationZMatrix(Yaw);
}

void FRotator::Normalize()
{
	Roll = MathHelper::NormalizeAngle(Roll);
	Pitch = MathHelper::NormalizeAngle(Pitch);
	Yaw = MathHelper::NormalizeAngle(Yaw);
}

FRotator FRotator::operator+(const FRotator& Other) const
{
	FRotator r{ Roll + Other.Roll, Pitch + Other.Pitch, Yaw + Other.Yaw };
	return r;
}

FRotator FRotator::operator-(const FRotator& Other) const
{
	FRotator r{ Roll - Other.Roll, Pitch - Other.Pitch, Yaw - Other.Yaw };
	return r;
}

FRotator& FRotator::operator+=(const FRotator& Other)
{
	Roll += Other.Roll;
	Pitch += Other.Pitch;
	Yaw += Other.Yaw;
	return *this;
}

FRotator& FRotator::operator-=(const FRotator& Other)
{
	Roll -= Other.Roll;
	Pitch -= Other.Pitch;
	Yaw -= Other.Yaw;
	return *this;
}

FRotator FRotator::FromVector(const FVector& Forward)
{
	float Yaw = MathHelper::Atan2(Forward.y, Forward.x) * (180.0f / MathHelper::PI);
	float Pitch = MathHelper::Atan2(Forward.z, MathHelper::Sqrt(Forward.x * Forward.x + Forward.y * Forward.y)) * (180.0f / MathHelper::PI);
	return {0.0f, Pitch, Yaw};
}

FRotator FRotator::ZeroRotator()
{
	return {0.0f, 0.0f, 0.0f};
}