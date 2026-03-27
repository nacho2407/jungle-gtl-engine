#include "Math/Rotator.h"

#include "Math/MathUtility.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"

#include <cassert>
#include <cmath>

const FRotator FRotator::ZeroRotator(0.0f, 0.0f, 0.0f);

FRotator::FRotator(const FQuat& InQuat) noexcept
	: FRotator(InQuat.Rotator())
{
}

float FRotator::ClampAxis(float AngleDegrees) noexcept
{
	float ClampedAngle = std::fmod(AngleDegrees, 360.0f);
	if (ClampedAngle < 0.0f)
	{
		ClampedAngle += 360.0f;
	}

	return ClampedAngle;
}

float FRotator::NormalizeAxis(float AngleDegrees) noexcept
{
	float NormalizedAngle = ClampAxis(AngleDegrees);
	if (NormalizedAngle > 180.0f)
	{
		NormalizedAngle -= 360.0f;
	}

	return NormalizedAngle;
}

FRotator FRotator::MakeFromEuler(const FVector& InEulerDegrees) noexcept
{
	return FRotator(InEulerDegrees.Y, InEulerDegrees.Z, InEulerDegrees.X);
}

bool FRotator::operator==(const FRotator& Other) const noexcept
{
	return Pitch == Other.Pitch && Yaw == Other.Yaw && Roll == Other.Roll;
}

bool FRotator::operator!=(const FRotator& Other) const noexcept
{
	return !(*this == Other);
}

FRotator FRotator::operator-() const noexcept
{
	return FRotator(-Pitch, -Yaw, -Roll);
}

FRotator FRotator::operator+(const FRotator& Other) const noexcept
{
	return FRotator(Pitch + Other.Pitch, Yaw + Other.Yaw, Roll + Other.Roll);
}

FRotator FRotator::operator-(const FRotator& Other) const noexcept
{
	return FRotator(Pitch - Other.Pitch, Yaw - Other.Yaw, Roll - Other.Roll);
}

FRotator FRotator::operator*(float Scale) const noexcept
{
	return FRotator(Pitch * Scale, Yaw * Scale, Roll * Scale);
}

FRotator FRotator::operator/(float Scale) const noexcept
{
	assert(std::fabs(Scale) > 1.e-8f);
	return FRotator(Pitch / Scale, Yaw / Scale, Roll / Scale);
}

FRotator& FRotator::operator+=(const FRotator& Other) noexcept
{
	Pitch += Other.Pitch;
	Yaw += Other.Yaw;
	Roll += Other.Roll;
	return *this;
}

FRotator& FRotator::operator-=(const FRotator& Other) noexcept
{
	Pitch -= Other.Pitch;
	Yaw -= Other.Yaw;
	Roll -= Other.Roll;
	return *this;
}

FRotator& FRotator::operator*=(float Scale) noexcept
{
	Pitch *= Scale;
	Yaw *= Scale;
	Roll *= Scale;
	return *this;
}

FRotator& FRotator::operator/=(float Scale) noexcept
{
	assert(std::fabs(Scale) > 1.e-8f);
	Pitch /= Scale;
	Yaw /= Scale;
	Roll /= Scale;
	return *this;
}

FVector FRotator::Euler() const noexcept
{
	return FVector(Roll, Pitch, Yaw);
}

FVector FRotator::Vector() const noexcept
{
	const float PitchRadians = FMath::DegreesToRadians(Pitch);
	const float YawRadians = FMath::DegreesToRadians(Yaw);

	const float CosPitch = std::cos(PitchRadians);
	return FVector(
		CosPitch * std::cos(YawRadians),
		CosPitch * std::sin(YawRadians),
		std::sin(PitchRadians)
	).GetSafeNormal();
}

FVector FRotator::RotateVector(const FVector& InVector) const noexcept
{
	return Quaternion().RotateVector(InVector);
}

FVector FRotator::UnrotateVector(const FVector& InVector) const noexcept
{
	return Quaternion().UnrotateVector(InVector);
}

FRotator FRotator::Add(float DeltaPitch, float DeltaYaw, float DeltaRoll) noexcept
{
	Pitch += DeltaPitch;
	Yaw += DeltaYaw;
	Roll += DeltaRoll;
	return *this;
}

bool FRotator::ContainsNaN() const noexcept
{
	return !std::isfinite(Pitch) || !std::isfinite(Yaw) || !std::isfinite(Roll);
}

bool FRotator::IsZero() const noexcept
{
	return NormalizeAxis(Pitch) == 0.0f
		&& NormalizeAxis(Yaw) == 0.0f
		&& NormalizeAxis(Roll) == 0.0f;
}

bool FRotator::Equals(const FRotator& Other, float Tolerance) const noexcept
{
	return std::fabs(NormalizeAxis(Pitch - Other.Pitch)) <= Tolerance
		&& std::fabs(NormalizeAxis(Yaw - Other.Yaw)) <= Tolerance
		&& std::fabs(NormalizeAxis(Roll - Other.Roll)) <= Tolerance;
}

bool FRotator::IsNearlyZero(float Tolerance) const noexcept
{
	return std::fabs(NormalizeAxis(Pitch)) <= Tolerance
		&& std::fabs(NormalizeAxis(Yaw)) <= Tolerance
		&& std::fabs(NormalizeAxis(Roll)) <= Tolerance;
}

float FRotator::GetManhattanDistance(const FRotator& Other) const noexcept
{
	return std::fabs(NormalizeAxis(Pitch - Other.Pitch))
		+ std::fabs(NormalizeAxis(Yaw - Other.Yaw))
		+ std::fabs(NormalizeAxis(Roll - Other.Roll));
}

void FRotator::SetClosestToMe(FRotator& MakeClosest) const noexcept
{
	MakeClosest.Pitch = Pitch + NormalizeAxis(MakeClosest.Pitch - Pitch);
	MakeClosest.Yaw = Yaw + NormalizeAxis(MakeClosest.Yaw - Yaw);
	MakeClosest.Roll = Roll + NormalizeAxis(MakeClosest.Roll - Roll);
}

void FRotator::Clamp() noexcept
{
	Pitch = ClampAxis(Pitch);
	Yaw = ClampAxis(Yaw);
	Roll = ClampAxis(Roll);
}

void FRotator::Normalize() noexcept
{
	Pitch = NormalizeAxis(Pitch);
	Yaw = NormalizeAxis(Yaw);
	Roll = NormalizeAxis(Roll);
}

FRotator FRotator::GetDenormalized() const noexcept
{
	FRotator Result = *this;
	Result.Clamp();
	return Result;
}

FRotator FRotator::GetNormalized() const noexcept
{
	FRotator Result = *this;
	Result.Normalize();
	return Result;
}

FRotator FRotator::GetInverse() const noexcept
{
	return Quaternion().Inverse().Rotator();
}

FQuat FRotator::Quaternion() const noexcept
{
	const FMatrix RotationMatrix =
		FMatrix::MakeRotationZ(FMath::DegreesToRadians(Yaw))
		* FMatrix::MakeRotationY(FMath::DegreesToRadians(Pitch))
		* FMatrix::MakeRotationX(FMath::DegreesToRadians(Roll));

	return FQuat(DirectX::XMQuaternionRotationMatrix(RotationMatrix.ToXMMatrix())).GetNormalized();
}
