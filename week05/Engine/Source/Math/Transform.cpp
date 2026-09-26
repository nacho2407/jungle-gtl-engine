#include "Math/Transform.h"

const FTransform FTransform::Identity(FQuat::Identity, FVector::ZeroVector, FVector::OneVector);

FTransform::FTransform(const FMatrix& InMatrix) noexcept
	: Rotation(FQuat::Identity)
	, Translation(FVector::ZeroVector)
	, Scale3D(FVector::OneVector)
{
	DirectX::XMVECTOR OutScale;
	DirectX::XMVECTOR OutRotation;
	DirectX::XMVECTOR OutTranslation;

	if (DirectX::XMMatrixDecompose(&OutScale, &OutRotation, &OutTranslation, InMatrix.ToXMMatrix()))
	{
		Scale3D = FVector(OutScale);
		Rotation = FQuat(OutRotation).GetNormalized();
		Translation = FVector(OutTranslation);
	}
}

const FVector& FTransform::GetLocation() const noexcept
{
	return Translation;
}

const FVector& FTransform::GetTranslation() const noexcept
{
	return Translation;
}

const FQuat& FTransform::GetRotation() const noexcept
{
	return Rotation;
}

const FVector& FTransform::GetScale3D() const noexcept
{
	return Scale3D;
}

void FTransform::SetLocation(const FVector& InTranslation) noexcept
{
	Translation = InTranslation;
}

void FTransform::SetTranslation(const FVector& InTranslation) noexcept
{
	Translation = InTranslation;
}

void FTransform::SetRotation(const FQuat& InRotation) noexcept
{
	Rotation = InRotation.GetNormalized();
}

void FTransform::SetRotation(const FRotator& InRotation) noexcept
{
	Rotation = InRotation.Quaternion();
}

void FTransform::SetScale3D(const FVector& InScale3D) noexcept
{
	Scale3D = InScale3D;
}

void FTransform::SetIdentity() noexcept
{
	Rotation = FQuat::Identity;
	Translation = FVector::ZeroVector;
	Scale3D = FVector::OneVector;
}

FRotator FTransform::Rotator() const noexcept
{
	return Rotation.Rotator();
}

void FTransform::NormalizeRotation() noexcept
{
	Rotation.Normalize();
}

bool FTransform::Equals(const FTransform& Other, float Tolerance) const noexcept
{
	return Translation.Equals(Other.Translation, Tolerance)
		&& Rotation.Equals(Other.Rotation, Tolerance)
		&& Scale3D.Equals(Other.Scale3D, Tolerance);
}

bool FTransform::IsIdentity(float Tolerance) const noexcept
{
	return Equals(Identity, Tolerance);
}

void FTransform::AddToTranslation(const FVector& DeltaTranslation) noexcept
{
	Translation += DeltaTranslation;
}

FVector FTransform::TransformPosition(const FVector& InPosition) const noexcept
{
	return Rotation.RotateVector(ComponentMultiply(InPosition, Scale3D)) + Translation;
}

FVector FTransform::TransformPositionNoScale(const FVector& InPosition) const noexcept
{
	return Rotation.RotateVector(InPosition) + Translation;
}

FVector FTransform::TransformVector(const FVector& InVector) const noexcept
{
	return Rotation.RotateVector(ComponentMultiply(InVector, Scale3D));
}

FVector FTransform::TransformVectorNoScale(const FVector& InVector) const noexcept
{
	return Rotation.RotateVector(InVector);
}

FVector FTransform::InverseTransformPosition(const FVector& InPosition) const noexcept
{
	const FVector Untranslated = InPosition - Translation;
	const FVector Unrotated = Rotation.UnrotateVector(Untranslated);
	return ComponentDivideSafe(Unrotated, Scale3D);
}

FVector FTransform::InverseTransformPositionNoScale(const FVector& InPosition) const noexcept
{
	return Rotation.UnrotateVector(InPosition - Translation);
}

FVector FTransform::InverseTransformVector(const FVector& InVector) const noexcept
{
	return ComponentDivideSafe(Rotation.UnrotateVector(InVector), Scale3D);
}

FVector FTransform::InverseTransformVectorNoScale(const FVector& InVector) const noexcept
{
	return Rotation.UnrotateVector(InVector);
}

FVector FTransform::GetUnitAxis(EAxis Axis) const noexcept
{
	return GetScaledAxis(Axis).GetSafeNormal();
}

FVector FTransform::GetScaledAxis(EAxis Axis) const noexcept
{
	switch (Axis)
	{
	case EAxis::X:
		return Rotation.RotateVector(FVector(Scale3D.X, 0.0f, 0.0f));
	case EAxis::Y:
		return Rotation.RotateVector(FVector(0.0f, Scale3D.Y, 0.0f));
	case EAxis::Z:
		return Rotation.RotateVector(FVector(0.0f, 0.0f, Scale3D.Z));
	default:
		return FVector::ZeroVector;
	}
}

FMatrix FTransform::ToMatrixNoScale() const noexcept
{
	return FMatrix::MakeWorld(Translation, Rotation.ToMatrix(), FVector::OneVector);
}

FMatrix FTransform::ToMatrixWithScale() const noexcept
{
	return FMatrix::MakeWorld(Translation, Rotation.ToMatrix(), Scale3D);
}

FMatrix FTransform::ToInverseMatrixWithScale() const noexcept
{
	return ToMatrixWithScale().GetInverse();
}

FMatrix FTransform::ToMatrix() const noexcept
{
	return ToMatrixWithScale();
}

FTransform FTransform::Inverse() const noexcept
{
	const FVector InverseScale3D = GetSafeScaleReciprocal(Scale3D);
	const FQuat InverseRotation = Rotation.Inverse();
	const FVector InverseTranslation =
		InverseRotation.RotateVector(ComponentMultiply(-Translation, InverseScale3D));

	return FTransform(InverseRotation, InverseTranslation, InverseScale3D);
}

FTransform FTransform::operator*(const FTransform& Other) const noexcept
{
	const FVector ResultScale3D = ComponentMultiply(Scale3D, Other.Scale3D);
	const FQuat ResultRotation = Rotation * Other.Rotation;
	const FVector ResultTranslation = Other.TransformPosition(Translation);

	return FTransform(ResultRotation, ResultTranslation, ResultScale3D);
}

FTransform& FTransform::operator*=(const FTransform& Other) noexcept
{
	*this = *this * Other;
	return *this;
}

FVector FTransform::ComponentMultiply(const FVector& A, const FVector& B) noexcept
{
	return FVector(DirectX::XMVectorMultiply(A.ToXMVector(), B.ToXMVector()));
}

FVector FTransform::ComponentDivideSafe(const FVector& A, const FVector& B, float Tolerance) noexcept
{
	const XMVector Numerator = A.ToXMVector();
	const XMVector Denominator = B.ToXMVector();
	const XMVector SafeMask = DirectX::XMVectorGreater(
		DirectX::XMVectorAbs(Denominator),
		DirectX::XMVectorReplicate(Tolerance));
	const XMVector Quotient = DirectX::XMVectorDivide(Numerator, Denominator);
	return FVector(DirectX::XMVectorSelect(DirectX::XMVectorZero(), Quotient, SafeMask));
}

FVector FTransform::GetSafeScaleReciprocal(const FVector& InScale, float Tolerance) noexcept
{
	return ComponentDivideSafe(FVector::OneVector, InScale, Tolerance);
}
