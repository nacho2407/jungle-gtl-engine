#include "Rotator.h"

const FRotator FRotator::ZeroRotator(0.0f, 0.0f, 0.0f);

FRotator FRotator::FromVector(const FVector& Forward)
{
	float Yaw = MathHelper::Atan2(Forward.y, Forward.x) * (180.0f / MathHelper::PI);
	float Pitch = MathHelper::Atan2(Forward.z, MathHelper::Sqrt(Forward.x * Forward.x + Forward.y * Forward.y)) * (180.0f / MathHelper::PI);
	return FRotator(Pitch, Yaw, 0.0f);
}

FMatrix FRotator::ToMatrix() const
{
	return FMatrix::RotationXMatrix(Roll) * FMatrix::RotationYMatrix(Pitch) * FMatrix::RotationZMatrix(Yaw);
}

void FRotator::Normalize()
{
	Pitch = MathHelper::NormalizeAngle(Pitch);
	Yaw = MathHelper::NormalizeAngle(Yaw);
	Roll = MathHelper::NormalizeAngle(Roll);
}

FRotator FRotator::operator+(const FRotator& Other) const
{ 
	FRotator r{ Pitch + Other.Pitch, Yaw + Other.Yaw, Roll + Other.Roll};
	r.Normalize();
	return r; 

}
FRotator FRotator::operator-(const FRotator& Other) const 
{ 
	FRotator r{ Pitch - Other.Pitch, Yaw - Other.Yaw, Roll - Other.Roll };
	r.Normalize();
	return r;
}

FRotator& FRotator::operator+=(const FRotator& Other) 
{ 
	Pitch += Other.Pitch; Yaw += Other.Yaw; Roll += Other.Roll;
	this->Normalize();
	return *this; 
}