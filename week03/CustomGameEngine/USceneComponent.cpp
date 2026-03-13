#include "USceneComponent.h"

USceneComponent::USceneComponent()
{
	RelativeLocation = FVector(0.0f, 0.0f, 0.0f);
	RelativeRotation = FVector(0.0f, 0.0f, 0.0f);
	RelativeScale3D = FVector(1.0f, 1.0f, 1.0f);
}

USceneComponent::~USceneComponent()
{

}

FMatrix USceneComponent::GetRelativeMatrix() const
{
	FMatrix S = FMatrix::ScaleMatrix(RelativeScale3D.x, RelativeScale3D.y, RelativeScale3D.z);
	FMatrix R = FMatrix::EulerRotationMatrix(RelativeRotation.x, RelativeRotation.y, RelativeRotation.z);
	FMatrix T = FMatrix::TranslationMatrix(RelativeLocation.x, RelativeLocation.y, RelativeLocation.z);

	return S * R * T;
}
