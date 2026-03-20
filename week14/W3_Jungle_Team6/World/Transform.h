#pragma once
#include "Engine/Math/Matrix.h"

struct FTransform
{
	FVector Location;
	FVector Rotation;
	FVector Scale;

	FTransform() : Location(0.0f, 0.0f, 0.0f), Rotation(0.0f, 0.0f, 0.0f), Scale(1.0f, 1.0f, 1.0f){}
	FTransform(const FVector& NewLocation, const FVector& NewRotation, const FVector& NewScale)
		: Location(NewLocation), Rotation(NewRotation), Scale(NewScale) {}

	FMatrix ToMatrix() const;
};