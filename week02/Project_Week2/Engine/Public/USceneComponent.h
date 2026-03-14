#pragma once
#include "Core.h"

class USceneComponent : public UObject
{
public:
	USceneComponent() : Location(0, 0, 0), Rotation(0, 0, 0), Scale(1, 1, 1) {};

	FVector GetLocation() const { return Location; }
	FVector SetLocation(const FVector& NewLocation) { Location = NewLocation; }

	FVector GetRotation() const { return Rotation; }
	FVector SetRotation(const FVector& NewRotation) { Rotation = NewRotation; }

	FVector GetScale() const { return Scale; }
	FVector SetScale(const FVector& NewScale) { Scale = NewScale; }

private:
	FVector Location;
	FVector Rotation;
	FVector Scale;


};