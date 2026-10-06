#include "Transform.h"

FTransform::FTransform(const FMatrix& Mat)
{
	Location = Mat.GetLocation();
	Rotation = Mat.ToQuat();
	Scale = Mat.GetScale();
}

FTransform FTransform::FromMatrixWithScale(const FMatrix& Mat)
{
	FTransform Result;
	Result.Location = Mat.GetLocation();
	Result.Scale = Mat.GetScale();

	FMatrix RotationMatrix = Mat;
	RotationMatrix.RemoveScaling();

	Result.Rotation = RotationMatrix.ToQuat().GetNormalized();
	return Result;
}

FMatrix FTransform::ToMatrix() const
{
	FMatrix translateMatrix = FMatrix::MakeTranslationMatrix(Location);

	FMatrix rotationMatrix = Rotation.ToMatrix();

	FMatrix scaleMatrix = FMatrix::MakeScaleMatrix(Scale);

	return scaleMatrix * rotationMatrix * translateMatrix;
}

FVector FTransform::TransformPosition(const FVector& Position) const
{
	return ToMatrix().TransformPosition(Position);
}

FVector FTransform::TransformVectorNoScale(const FVector& Vector) const
{
	return Rotation.RotateVector(Vector);
}

FVector FTransform::TransformPositionNoScale(const FVector& Position) const
{
	return Location + TransformVectorNoScale(Position);
}

FVector FTransform::InverseTransformPositionNoScale(const FVector& Position) const
{
	return Rotation.Inverse().RotateVector(Position - Location);
}

FTransform FTransform::operator*(const FTransform& Other) const
{
	return FTransform(ToMatrix() * Other.ToMatrix());
}

FTransform& FTransform::operator*=(const FTransform& Other)
{
	*this = *this * Other;
	return *this;
}
