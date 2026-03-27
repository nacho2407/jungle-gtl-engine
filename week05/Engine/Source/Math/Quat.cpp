#include "Math/Quat.h"

#include "Math/MathUtility.h"
#include "Math/Rotator.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace
{
	constexpr float MatrixConversionTolerance = FMath::SmallNumber;

	bool BuildOrthonormalBasisFromXY(
		const FVector& InX,
		const FVector& InY,
		FVector& OutX,
		FVector& OutY,
		FVector& OutZ) noexcept
	{
		OutX = InX.GetSafeNormal(MatrixConversionTolerance);
		if (OutX.IsNearlyZero(MatrixConversionTolerance))
		{
			return false;
		}

		const FVector ProjectedY = InY - OutX * FVector::DotProduct(InY, OutX);
		OutY = ProjectedY.GetSafeNormal(MatrixConversionTolerance);
		if (OutY.IsNearlyZero(MatrixConversionTolerance))
		{
			return false;
		}

		OutZ = FVector::CrossProduct(OutX, OutY).GetSafeNormal(MatrixConversionTolerance);
		if (OutZ.IsNearlyZero(MatrixConversionTolerance))
		{
			return false;
		}

		OutY = FVector::CrossProduct(OutZ, OutX).GetSafeNormal(MatrixConversionTolerance);
		return !OutY.IsNearlyZero(MatrixConversionTolerance);
	}

	bool BuildOrthonormalBasisFromXZ(
		const FVector& InX,
		const FVector& InZ,
		FVector& OutX,
		FVector& OutY,
		FVector& OutZ) noexcept
	{
		OutX = InX.GetSafeNormal(MatrixConversionTolerance);
		if (OutX.IsNearlyZero(MatrixConversionTolerance))
		{
			return false;
		}

		const FVector ProjectedZ = InZ - OutX * FVector::DotProduct(InZ, OutX);
		OutZ = ProjectedZ.GetSafeNormal(MatrixConversionTolerance);
		if (OutZ.IsNearlyZero(MatrixConversionTolerance))
		{
			return false;
		}

		OutY = FVector::CrossProduct(OutZ, OutX).GetSafeNormal(MatrixConversionTolerance);
		if (OutY.IsNearlyZero(MatrixConversionTolerance))
		{
			return false;
		}

		OutZ = FVector::CrossProduct(OutX, OutY).GetSafeNormal(MatrixConversionTolerance);
		return !OutZ.IsNearlyZero(MatrixConversionTolerance);
	}

	bool BuildOrthonormalBasisFromYZ(
		const FVector& InY,
		const FVector& InZ,
		FVector& OutX,
		FVector& OutY,
		FVector& OutZ) noexcept
	{
		OutY = InY.GetSafeNormal(MatrixConversionTolerance);
		if (OutY.IsNearlyZero(MatrixConversionTolerance))
		{
			return false;
		}

		const FVector ProjectedZ = InZ - OutY * FVector::DotProduct(InZ, OutY);
		OutZ = ProjectedZ.GetSafeNormal(MatrixConversionTolerance);
		if (OutZ.IsNearlyZero(MatrixConversionTolerance))
		{
			return false;
		}

		OutX = FVector::CrossProduct(OutY, OutZ).GetSafeNormal(MatrixConversionTolerance);
		if (OutX.IsNearlyZero(MatrixConversionTolerance))
		{
			return false;
		}

		OutZ = FVector::CrossProduct(OutX, OutY).GetSafeNormal(MatrixConversionTolerance);
		return !OutZ.IsNearlyZero(MatrixConversionTolerance);
	}
}

const FQuat FQuat::Identity(0.0f, 0.0f, 0.0f, 1.0f);

FQuat::FQuat(FXMVector InVector) noexcept
	: X(0.0f)
	, Y(0.0f)
	, Z(0.0f)
	, W(1.0f)
{
	DirectX::XMFLOAT4 Temp;
	DirectX::XMStoreFloat4(&Temp, InVector);

	X = Temp.x;
	Y = Temp.y;
	Z = Temp.z;
	W = Temp.w;
}

FQuat::FQuat(const FRotator& InRotator) noexcept
	: FQuat(InRotator.Quaternion())
{
}

FQuat::FQuat(const FMatrix& InMatrix) noexcept
	: X(0.0f)
	, Y(0.0f)
	, Z(0.0f)
	, W(1.0f)
{
	DirectX::XMVECTOR OutScale;
	DirectX::XMVECTOR OutRotation;
	DirectX::XMVECTOR OutTranslation;

	if (DirectX::XMMatrixDecompose(&OutScale, &OutRotation, &OutTranslation, InMatrix.ToXMMatrix()))
	{
		*this = FQuat(OutRotation);
		Normalize();
		return;
	}

	const FMatrix RotationSource = InMatrix.GetMatrixWithoutTranslation();
	const FVector XAxis = RotationSource.GetScaledAxis(EAxis::X);
	const FVector YAxis = RotationSource.GetScaledAxis(EAxis::Y);
	const FVector ZAxis = RotationSource.GetScaledAxis(EAxis::Z);

	FVector OrthoX;
	FVector OrthoY;
	FVector OrthoZ;

	if (BuildOrthonormalBasisFromXY(XAxis, YAxis, OrthoX, OrthoY, OrthoZ)
		|| BuildOrthonormalBasisFromXZ(XAxis, ZAxis, OrthoX, OrthoY, OrthoZ)
		|| BuildOrthonormalBasisFromYZ(YAxis, ZAxis, OrthoX, OrthoY, OrthoZ))
	{
		FMatrix OrthonormalMatrix = FMatrix::Identity;
		OrthonormalMatrix.SetAxes(OrthoX, OrthoY, OrthoZ);
		*this = FQuat(DirectX::XMQuaternionRotationMatrix(OrthonormalMatrix.ToXMMatrix()));
		Normalize();
		return;
	}

	*this = Identity;
}

FQuat::FQuat(const FVector& Axis, float AngleRad) noexcept
	: X(0.0f)
	, Y(0.0f)
	, Z(0.0f)
	, W(1.0f)
{
	const FVector NormalizedAxis = Axis.GetSafeNormal();
	if (!NormalizedAxis.IsNearlyZero())
	{
		*this = FQuat(DirectX::XMQuaternionRotationAxis(NormalizedAxis.ToXMVector(), AngleRad));
		Normalize();
	}
}

FQuat FQuat::MakeFromEuler(const FVector& InEulerDegrees) noexcept
{
	return FQuat(FRotator::MakeFromEuler(InEulerDegrees));
}

float FQuat::DotProduct(const FQuat& A, const FQuat& B) noexcept
{
	return DirectX::XMVectorGetX(DirectX::XMVector4Dot(A.ToXMVector(), B.ToXMVector()));
}

FQuat FQuat::Slerp(const FQuat& A, const FQuat& B, float Alpha) noexcept
{
	FQuat AdjustedB = B;
	if (DotProduct(A, B) < 0.0f)
	{
		AdjustedB = -AdjustedB;
	}

	return FQuat(DirectX::XMQuaternionSlerp(A.GetNormalized().ToXMVector(), AdjustedB.GetNormalized().ToXMVector(), Alpha)).GetNormalized();
}

bool FQuat::operator==(const FQuat& Other) const noexcept
{
	return X == Other.X && Y == Other.Y && Z == Other.Z && W == Other.W;
}

bool FQuat::operator!=(const FQuat& Other) const noexcept
{
	return !(*this == Other);
}

FQuat FQuat::operator-() const noexcept
{
	return FQuat(-X, -Y, -Z, -W);
}

FQuat FQuat::operator+(const FQuat& Other) const noexcept
{
	return FQuat(X + Other.X, Y + Other.Y, Z + Other.Z, W + Other.W);
}

FQuat FQuat::operator-(const FQuat& Other) const noexcept
{
	return FQuat(X - Other.X, Y - Other.Y, Z - Other.Z, W - Other.W);
}

FQuat FQuat::operator*(float Scale) const noexcept
{
	return FQuat(X * Scale, Y * Scale, Z * Scale, W * Scale);
}

FQuat FQuat::operator/(float Scale) const noexcept
{
	assert(std::fabs(Scale) > 1.e-8f);
	return FQuat(X / Scale, Y / Scale, Z / Scale, W / Scale);
}

FQuat FQuat::operator*(const FQuat& Other) const noexcept
{
	return FQuat(DirectX::XMQuaternionMultiply(ToXMVector(), Other.ToXMVector()));
}

FVector FQuat::operator*(const FVector& InVector) const noexcept
{
	return RotateVector(InVector);
}

FQuat& FQuat::operator+=(const FQuat& Other) noexcept
{
	X += Other.X;
	Y += Other.Y;
	Z += Other.Z;
	W += Other.W;
	return *this;
}

FQuat& FQuat::operator-=(const FQuat& Other) noexcept
{
	X -= Other.X;
	Y -= Other.Y;
	Z -= Other.Z;
	W -= Other.W;
	return *this;
}

FQuat& FQuat::operator*=(float Scale) noexcept
{
	X *= Scale;
	Y *= Scale;
	Z *= Scale;
	W *= Scale;
	return *this;
}

FQuat& FQuat::operator/=(float Scale) noexcept
{
	assert(std::fabs(Scale) > 1.e-8f);
	X /= Scale;
	Y /= Scale;
	Z /= Scale;
	W /= Scale;
	return *this;
}

FQuat& FQuat::operator*=(const FQuat& Other) noexcept
{
	*this = *this * Other;
	return *this;
}

float FQuat::operator|(const FQuat& Other) const noexcept
{
	return DotProduct(*this, Other);
}

XMVector FQuat::ToXMVector() const noexcept
{
	return DirectX::XMVectorSet(X, Y, Z, W);
}

bool FQuat::Equals(const FQuat& Other, float Tolerance) const noexcept
{
	const bool bSameSign = std::fabs(X - Other.X) <= Tolerance
		&& std::fabs(Y - Other.Y) <= Tolerance
		&& std::fabs(Z - Other.Z) <= Tolerance
		&& std::fabs(W - Other.W) <= Tolerance;

	const bool bNegatedSign = std::fabs(X + Other.X) <= Tolerance
		&& std::fabs(Y + Other.Y) <= Tolerance
		&& std::fabs(Z + Other.Z) <= Tolerance
		&& std::fabs(W + Other.W) <= Tolerance;

	return bSameSign || bNegatedSign;
}

bool FQuat::IsIdentity(float Tolerance) const noexcept
{
	return Equals(Identity, Tolerance);
}

bool FQuat::ContainsNaN() const noexcept
{
	return !std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z) || !std::isfinite(W);
}

float FQuat::SizeSquared() const noexcept
{
	return DirectX::XMVectorGetX(DirectX::XMVector4LengthSq(ToXMVector()));
}

float FQuat::Size() const noexcept
{
	return DirectX::XMVectorGetX(DirectX::XMVector4Length(ToXMVector()));
}

bool FQuat::IsNormalized(float Tolerance) const noexcept
{
	return std::fabs(SizeSquared() - 1.0f) <= Tolerance;
}

void FQuat::Normalize(float Tolerance) noexcept
{
	const XMVector QuatVector = ToXMVector();
	const float SquaredSize = DirectX::XMVectorGetX(DirectX::XMVector4LengthSq(QuatVector));
	if (SquaredSize > Tolerance)
	{
		*this = FQuat(DirectX::XMQuaternionNormalize(QuatVector));
		return;
	}

	*this = Identity;
}

FQuat FQuat::GetNormalized(float Tolerance) const noexcept
{
	FQuat Result = *this;
	Result.Normalize(Tolerance);
	return Result;
}

FQuat FQuat::Conjugate() const noexcept
{
	return FQuat(DirectX::XMQuaternionConjugate(ToXMVector()));
}

FQuat FQuat::Inverse() const noexcept
{
	const float SquaredSize = SizeSquared();
	if (SquaredSize <= 1.e-8f)
	{
		return Identity;
	}

	return Conjugate() / SquaredSize;
}

FVector FQuat::RotateVector(const FVector& InVector) const noexcept
{
	return FVector(DirectX::XMVector3Rotate(InVector.ToXMVector(), GetNormalized().ToXMVector()));
}

FVector FQuat::UnrotateVector(const FVector& InVector) const noexcept
{
	return FVector(DirectX::XMVector3InverseRotate(InVector.ToXMVector(), GetNormalized().ToXMVector()));
}

float FQuat::GetAngle() const noexcept
{
	const FQuat NormalizedQuat = GetNormalized();
	const float ClampedW = std::clamp(NormalizedQuat.W, -1.0f, 1.0f);
	return 2.0f * std::acos(ClampedW);
}

FVector FQuat::GetRotationAxis(float Tolerance) const noexcept
{
	const FQuat NormalizedQuat = GetNormalized();
	const float AxisSquared = NormalizedQuat.X * NormalizedQuat.X + NormalizedQuat.Y * NormalizedQuat.Y + NormalizedQuat.Z * NormalizedQuat.Z;
	if (AxisSquared <= Tolerance)
	{
		return FVector::ForwardVector;
	}

	const float InvAxisSize = 1.0f / std::sqrt(AxisSquared);
	return FVector(
		NormalizedQuat.X * InvAxisSize,
		NormalizedQuat.Y * InvAxisSize,
		NormalizedQuat.Z * InvAxisSize);
}

FVector FQuat::Euler() const noexcept
{
	return Rotator().Euler();
}

FVector FQuat::GetAxisX() const noexcept
{
	return RotateVector(FVector::ForwardVector);
}

FVector FQuat::GetAxisY() const noexcept
{
	return RotateVector(FVector::RightVector);
}

FVector FQuat::GetAxisZ() const noexcept
{
	return RotateVector(FVector::UpVector);
}

FVector FQuat::GetForwardVector() const noexcept
{
	return GetAxisX();
}

FVector FQuat::GetRightVector() const noexcept
{
	return GetAxisY();
}

FVector FQuat::GetUpVector() const noexcept
{
	return GetAxisZ();
}

float FQuat::AngularDistance(const FQuat& Other) const noexcept
{
	const float ClampedAbsDot = std::clamp(std::fabs(DotProduct(GetNormalized(), Other.GetNormalized())), -1.0f, 1.0f);
	return 2.0f * std::acos(ClampedAbsDot);
}

void FQuat::EnforceShortestArcWith(const FQuat& Other) noexcept
{
	if (DotProduct(*this, Other) < 0.0f)
	{
		X = -X;
		Y = -Y;
		Z = -Z;
		W = -W;
	}
}

FMatrix FQuat::ToMatrix() const noexcept
{
	return FMatrix(DirectX::XMMatrixRotationQuaternion(GetNormalized().ToXMVector()));
}

FRotator FQuat::Rotator() const noexcept
{
	const FMatrix RotationMatrix = ToMatrix();
	const float ClampedPitchSin = std::clamp(RotationMatrix.M[2][0], -1.0f, 1.0f);
	const float PitchRadians = std::asin(ClampedPitchSin);
	const float CosPitch = std::cos(PitchRadians);

	float YawRadians = 0.0f;
	float RollRadians = 0.0f;

	if (std::fabs(CosPitch) > 1.e-6f)
	{
		YawRadians = std::atan2(-RotationMatrix.M[1][0], RotationMatrix.M[0][0]);
		RollRadians = std::atan2(-RotationMatrix.M[2][1], RotationMatrix.M[2][2]);
	}
	else
	{
		YawRadians = std::atan2(RotationMatrix.M[0][1], RotationMatrix.M[1][1]);
	}

	FRotator Result(
		FMath::RadiansToDegrees(PitchRadians),
		FMath::RadiansToDegrees(YawRadians),
		FMath::RadiansToDegrees(RollRadians));
	Result.Normalize();
	return Result;
}
