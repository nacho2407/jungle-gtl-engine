#include "Transform.h"

FTransform::FTransform(const FMatrix& Mat)
{
	Location = Mat.GetLocation();
	Rotation = Mat.ToQuat();
	Scale = Mat.GetScale();
}

FMatrix FTransform::ToMatrix() const
{
	FMatrix translateMatrix = FMatrix::MakeTranslationMatrix(Location);

	FMatrix rotationMatrix = Rotation.ToMatrix();

	FMatrix scaleMatrix = FMatrix::MakeScaleMatrix(Scale);

	return scaleMatrix * rotationMatrix * translateMatrix;
}
