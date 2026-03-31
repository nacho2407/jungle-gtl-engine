#pragma once
#include "Engine/Math/Matrix.h"
#include "Engine/Math/Rotator.h"

struct FTransform
{
	FVector Location;
	FRotator Rotation;
	FVector Scale;

	FTransform() : Location(0.0f, 0.0f, 0.0f), Rotation(), Scale(1.0f, 1.0f, 1.0f){}
	FTransform(const FVector& NewLocation, const FRotator& NewRotation, const FVector& NewScale)
		: Location(NewLocation), Rotation(NewRotation), Scale(NewScale) {}

	// FVector 오일러 호환 (X=Roll, Y=Pitch, Z=Yaw)
	FTransform(const FVector& NewLocation, const FVector& EulerRotation, const FVector& NewScale)
		: Location(NewLocation), Rotation(EulerRotation), Scale(NewScale) {}

	FMatrix ToMatrix() const;
};