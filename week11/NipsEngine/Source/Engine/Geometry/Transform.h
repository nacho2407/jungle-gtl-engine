#pragma once
#pragma once

#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"

struct FTransform
{
public:
	static const FTransform Identity;

	FTransform() noexcept = default;

	explicit FTransform(const FQuat& InRotation) noexcept
		: Rotation(InRotation.GetNormalized())
	{
	}

	explicit FTransform(const FRotator& InRotation) noexcept
		: Rotation(InRotation.Quaternion())
	{
	}

	FTransform(const FQuat& InRotation, const FVector& InTranslation, const FVector& InScale3D = FVector::OneVector) noexcept
		: Rotation(InRotation.GetNormalized())
		, Translation(InTranslation)
		, Scale3D(InScale3D)
	{
	}

	FTransform(const FRotator& InRotation, const FVector& InTranslation, const FVector& InScale3D = FVector::OneVector) noexcept
		: Rotation(InRotation.Quaternion())
		, Translation(InTranslation)
		, Scale3D(InScale3D)
	{
	}

	explicit FTransform(const FMatrix& InMatrix) noexcept;

	const FVector& GetLocation() const noexcept;
	const FVector& GetTranslation() const noexcept;
	const FQuat& GetRotation() const noexcept;
	const FVector& GetScale3D() const noexcept;

	void SetLocation(const FVector& InTranslation) noexcept;
	void SetTranslation(const FVector& InTranslation) noexcept;
	void SetRotation(const FQuat& InRotation) noexcept;
	void SetRotation(const FRotator& InRotation) noexcept;
	void SetScale3D(const FVector& InScale3D) noexcept;
	void SetIdentity() noexcept;

	FRotator Rotator() const noexcept;
	void NormalizeRotation() noexcept;
	bool Equals(const FTransform& Other, float Tolerance = 1.e-6f) const noexcept;
	bool IsIdentity(float Tolerance = 1.e-6f) const noexcept;
	void AddToTranslation(const FVector& DeltaTranslation) noexcept;

	FVector TransformPosition(const FVector& InPosition) const noexcept;
	FVector TransformPositionNoScale(const FVector& InPosition) const noexcept;
	FVector TransformVector(const FVector& InVector) const noexcept;
	FVector TransformVectorNoScale(const FVector& InVector) const noexcept;

	FVector InverseTransformPosition(const FVector& InPosition) const noexcept;
	FVector InverseTransformPositionNoScale(const FVector& InPosition) const noexcept;
	FVector InverseTransformVector(const FVector& InVector) const noexcept;
	FVector InverseTransformVectorNoScale(const FVector& InVector) const noexcept;

	FVector GetUnitAxis(EAxis Axis) const noexcept;
	FVector GetScaledAxis(EAxis Axis) const noexcept;

	FMatrix ToMatrixNoScale() const noexcept;
	FMatrix ToMatrixWithScale() const noexcept;
	FMatrix ToInverseMatrixWithScale() const noexcept;
	FMatrix ToMatrix() const noexcept;
	FTransform Inverse() const noexcept;

	FTransform operator*(const FTransform& Other) const noexcept;
	FTransform& operator*=(const FTransform& Other) noexcept;

private:
	static FVector ComponentMultiply(const FVector& A, const FVector& B) noexcept;
	static FVector ComponentDivideSafe(const FVector& A, const FVector& B, float Tolerance = 1.e-8f) noexcept;
	static FVector GetSafeScaleReciprocal(const FVector& InScale, float Tolerance = 1.e-8f) noexcept;

private:
	FQuat Rotation = FQuat::Identity;
	FVector Translation = FVector::ZeroVector;
	FVector Scale3D = FVector::OneVector;
};
